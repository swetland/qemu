// vexriscv-intc.c - vesxriscv compatible interrupt controller
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
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "hw/intc/vexriscv-intc.h"
#include "target/riscv/cpu.h"

static void vexriscv_intc_update(VexRiscvIntcState* s) {
	bool m_level = !!(s->irq_pending_bits & s->irq_m_enable_bits);
	bool s_level = !!(s->irq_pending_bits & s->irq_s_enable_bits);
	qemu_set_irq(s->external_irqs[0], m_level);
	qemu_set_irq(s->external_irqs[1], s_level);
}

static void vexriscv_intc_irq_request(void *opaque, int irq, int level) {
	VexRiscvIntcState *s = opaque;
	uint32_t bit = 1U << irq;
	if (level > 0) {
		qatomic_or(&s->irq_pending_bits, bit);
	} else {
		qatomic_and(&s->irq_pending_bits, ~bit);
	}
	vexriscv_intc_update(s);
}

static void vexriscv_intc_reset(DeviceState *dev) {
	VexRiscvIntcState *s = VEXRISCV_INTC(dev);
	s->irq_m_enable_bits = 0;
	s->irq_s_enable_bits = 0;
	s->irq_pending_bits = 0;
	vexriscv_intc_update(s);
}

static void vexriscv_intc_realize(DeviceState *dev, Error **error) {
	VexRiscvIntcState *s = VEXRISCV_INTC(dev);

	// support 32 interrupts inbound
	qdev_init_gpio_in(dev, vexriscv_intc_irq_request, 32);

	// route to M_EXT and S_EXT interrupts on the CPU
	s->external_irqs = g_malloc(sizeof(qemu_irq) * 2);
	qdev_init_gpio_out(dev, s->external_irqs, 2);

	vexriscv_intc_reset(dev);
}

static void vexriscv_intc_class_init(ObjectClass *oc, void* data) {
	DeviceClass *dc = DEVICE_CLASS(oc);
	dc->reset = vexriscv_intc_reset;
	dc->realize = vexriscv_intc_realize;
}

static const TypeInfo vexriscv_intc_info = {
	.name = TYPE_VEXRISCV_INTC,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(VexRiscvIntcState),
	.class_init = vexriscv_intc_class_init,
};

static void vexriscv_intc_register_types(void) {
	type_register_static(&vexriscv_intc_info);
}

type_init(vexriscv_intc_register_types);

// todo: is there a better way to get this from csr ops?
static VexRiscvIntcState* vri_state = NULL;

static RISCVException vexriscv_m_intc_enable_rd(CPURISCVState *s, int csrno, target_ulong *val) {
	*val = qatomic_read(&vri_state->irq_m_enable_bits);
	return RISCV_EXCP_NONE;
}

static RISCVException vexriscv_s_intc_enable_rd(CPURISCVState *s, int csrno, target_ulong *val) {
	*val = qatomic_read(&vri_state->irq_s_enable_bits);
	return RISCV_EXCP_NONE;
}

static RISCVException vexriscv_m_intc_enable_wr(CPURISCVState *s, int csrno, target_ulong val) {
	qatomic_set(&vri_state->irq_m_enable_bits, val);
	vexriscv_intc_update(vri_state);
	return RISCV_EXCP_NONE;
}

static RISCVException vexriscv_s_intc_enable_wr(CPURISCVState *s, int csrno, target_ulong val) {
	qatomic_set(&vri_state->irq_s_enable_bits, val);
	vexriscv_intc_update(vri_state);
	return RISCV_EXCP_NONE;
}

static RISCVException vexriscv_intc_pending_rd(CPURISCVState *s, int csrno, target_ulong *val) {
	*val = qatomic_read(&vri_state->irq_pending_bits);
	return RISCV_EXCP_NONE;
}

static RISCVException always(CPURISCVState *s, int csrno) {
	return RISCV_EXCP_NONE;
}

static riscv_csr_operations vexriscv_m_intc_enable_ops = {
	.name = "mintcenable",
	.predicate = always,
	.read = vexriscv_m_intc_enable_rd,
	.write = vexriscv_m_intc_enable_wr,
};

static riscv_csr_operations vexriscv_s_intc_enable_ops = {
	.name = "sintcenable",
	.predicate = always,
	.read = vexriscv_s_intc_enable_rd,
	.write = vexriscv_s_intc_enable_wr,
};

static riscv_csr_operations vexriscv_m_intc_pending_ops = {
	.name = "mintcenable",
	.predicate = always,
	.read = vexriscv_intc_pending_rd,
};

static riscv_csr_operations vexriscv_s_intc_pending_ops = {
	.name = "sintcpending",
	.predicate = always,
	.read = vexriscv_intc_pending_rd,
};

DeviceState *vexriscv_intc_create(void) {
	DeviceState *dev = qdev_new(TYPE_VEXRISCV_INTC);

	sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

	// todo: handle multiple harts
	CPUState *cpu = qemu_get_cpu(0);

	qdev_connect_gpio_out(dev, 0, qdev_get_gpio_in(DEVICE(cpu), IRQ_M_EXT));
	qdev_connect_gpio_out(dev, 1, qdev_get_gpio_in(DEVICE(cpu), IRQ_S_EXT));

	riscv_set_csr_ops(CSR_M_INTC_ENABLE, &vexriscv_m_intc_enable_ops);
	riscv_set_csr_ops(CSR_M_INTC_PENDING, &vexriscv_m_intc_pending_ops);
	riscv_set_csr_ops(CSR_S_INTC_ENABLE, &vexriscv_s_intc_enable_ops);
	riscv_set_csr_ops(CSR_S_INTC_PENDING, &vexriscv_s_intc_pending_ops);

	vri_state = VEXRISCV_INTC(dev);
	return dev;
}
