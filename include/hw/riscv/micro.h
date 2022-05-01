// micro.h - minimal riscv32 machine
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

#ifndef HW_RISCV_MICRO_H
#define HW_RISCV_MICRO_H

#include "hw/riscv/riscv_hart.h"
#include "hw/boards.h"

#define MICRO_CPUS_MAX 1
#define MICRO_SOCKETS_MAX 1

#define TYPE_RISCV_MICRO_MACHINE MACHINE_TYPE_NAME("micro")

#define RISCV_MICRO_MACHINE(obj) \
	OBJECT_CHECK(MicroMachineState, (obj), TYPE_RISCV_MICRO_MACHINE)

typedef struct MicroMachineState {
	MachineState parent;

	RISCVHartArrayState soc[MICRO_SOCKETS_MAX];
	DeviceState *intc;
	MemoryRegion rom;
} MicroMachineState;

enum {
	MICRO_ROM,
	MICRO_DRAM,
	MICRO_UART0,
};

enum {
	UART0_IRQ = 0,
	TIMER0_IRQ = 1,
};

#endif
