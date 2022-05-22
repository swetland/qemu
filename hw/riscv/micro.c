// micro.c - minimal riscv32 machine
//
// Copyright (c) 2022 Brian Swetland <swetland@frotz.net>
//
// Permission to use, copy, modify, and/or distribute this software
// for any purpose with or without fee is hereby granted.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
// WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
// THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
// NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/micro.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/numa.h"
#include "hw/intc/vexriscv-intc.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "hw/char/litex-uart.h"
#include "hw/display/litex-fb.h"
#include "hw/timer/litex-timer.h"
#include "hw/net/liteeth.h"

void litex_ethmac_create(MemoryRegion *mr, hwaddr base0, hwaddr base1, qemu_irq irq);

void litex_ethmac_create(MemoryRegion *mr, hwaddr base0, hwaddr base1, qemu_irq irq) {
	DeviceState *dev = qdev_new(TYPE_LITEETH);
	SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

	NICInfo *nd = &nd_table[0];
	qemu_check_nic_model(nd, TYPE_LITEETH);
	qdev_set_nic_properties(dev, nd);

	sysbus_realize_and_unref(sbd, &error_fatal);
	memory_region_add_subregion(mr, base0, sysbus_mmio_get_region(sbd, 0));
	memory_region_add_subregion(mr, base1, sysbus_mmio_get_region(sbd, 1));
	sysbus_connect_irq(sbd, 0, irq);

}

static const struct MemmapEntry {
	hwaddr base;
	hwaddr size;
} memmap[] = {
	[MICRO_ROM]         = { 0x00001000, 0x2000 },
	[MICRO_DRAM]        = { 0x40000000, 0x0 },
        [MICRO_ETHMAC_SRAM] = { 0xE0000000, 0x2000 },
	[MICRO_TIMER0]      = { 0xF0002000, 0x100 },
	[MICRO_UART0]       = { 0xF0002800, 0x100 },
	[MICRO_ETHMAC]      = { 0xF0005800, 0x100 },
};

// MicroMachineState

static void micro_machine_init(MachineState *ms) {
	MicroMachineState *mms = RISCV_MICRO_MACHINE(ms);
	MemoryRegion *sysmem = get_system_memory();
	target_ulong fw_end, kstart, kentry;

	// default: boot from start of ram
	target_ulong start_addr = memmap[MICRO_DRAM].base;

	if (riscv_socket_count(ms) != 1) {
		error_report("machine 'micro' only supports 1 socket");
		exit(1);
	}
	if (riscv_socket_hart_count(ms, 0) != 1) {
		error_report("machine 'micro' only supports 1 hart");
		exit(1);
	}

	RISCVHartArrayState *soc = &mms->soc[0];
	object_initialize_child(OBJECT(ms), "soc0", soc, TYPE_RISCV_HART_ARRAY);
	object_property_set_str(OBJECT(soc), "cpu-type", ms->cpu_type, &error_abort);
	object_property_set_int(OBJECT(soc), "hartid-base", 0, &error_abort);
	object_property_set_int(OBJECT(soc), "num-harts", 1, &error_abort);
	sysbus_realize(SYS_BUS_DEVICE(soc), &error_abort);

	// Main Memory
	memory_region_add_subregion(sysmem, memmap[MICRO_DRAM].base, ms->ram);

	// Boot Rom
	memory_region_init_rom(&mms->rom, NULL, "riscv.micro.rom",
	                       memmap[MICRO_ROM].size, &error_fatal);
	memory_region_add_subregion(sysmem, memmap[MICRO_ROM].base, &mms->rom);


	if (ms->firmware) {
		fw_end = riscv_load_firmware(ms->firmware, start_addr, NULL);
	} else {
		fw_end = start_addr;
	}

	if (ms->kernel_filename) {
		kstart = riscv_calc_kernel_start_addr(soc, fw_end);
		kstart = start_addr + 0x8000;
		kentry = riscv_load_kernel(ms->kernel_filename, kstart, NULL);
#if 0
		if (ms->initrd_filename) {
			hwaddr start, end;
			end = riscv_load_initrd(ms->initrd_filename, ms->ram_size,
			                        kentry, &start);
			// todo: tell kernel where the initrd went
		}
#endif
	} else {
		kentry = 0;
	}

	// Create Simple Local Interrupt Controller
	mms->intc = vexriscv_intc_create();

	litex_timer_create(sysmem, memmap[MICRO_TIMER0].base,
	                  qdev_get_gpio_in(DEVICE(mms->intc), TIMER0_IRQ));

	litex_uart_create(sysmem, memmap[MICRO_UART0].base, serial_hd(0),
	                  qdev_get_gpio_in(DEVICE(mms->intc), UART0_IRQ));

	litex_fb_create(ms->ram, 0x00c00000);

	litex_ethmac_create(sysmem, memmap[MICRO_ETHMAC].base,
			memmap[MICRO_ETHMAC_SRAM].base,
			qdev_get_gpio_in(DEVICE(mms->intc), ETHMAC_IRQ));

	riscv_setup_rom_reset_vec(ms, soc, start_addr,
	                          memmap[MICRO_ROM].base,
	                          memmap[MICRO_ROM].size,
	                          kentry, /* fdt */ 0, NULL);
}

// todo: add a shutdown peripheral:
// qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);

static void micro_machine_instance_init(Object *obj) {}

static void micro_machine_class_init(ObjectClass *oc, void *data) {
	MachineClass *mc = MACHINE_CLASS(oc);
	mc->desc = "RISC-V Board compatible-ish with VexRiscv/Litex";
	mc->init = micro_machine_init;
	mc->default_cpu_type = TYPE_RISCV_CPU_BASE32;
	mc->default_ram_id = "riscv.micro.ram";
	mc->max_cpus = MICRO_CPUS_MAX;
	mc->is_default = true;
	mc->numa_mem_supported = false;
}

static const TypeInfo micro_machine_type_info = {
	.name = TYPE_RISCV_MICRO_MACHINE,
	.parent = TYPE_MACHINE,
	.class_init = micro_machine_class_init,
	.instance_init = micro_machine_instance_init,
	.instance_size = sizeof(MicroMachineState),
};

static void micro_machine_type_info_register(void) {
	type_register_static(&micro_machine_type_info);
}

type_init(micro_machine_type_info_register);
