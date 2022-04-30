// vexriscv-intc.h - vesxriscv compatible interrupt controller
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

#ifndef HW_INTC_VEXRISCV_INTC
#define HW_INTC_VEXRISCV_INTC

#include "hw/sysbus.h"
#include "qom/object.h"

typedef struct VexRiscvIntcState {
	SysBusDevice parent_obj;

	uint32_t irq_m_enable_bits;
	uint32_t irq_s_enable_bits;
	uint32_t irq_pending_bits;

	qemu_irq *external_irqs;
} VexRiscvIntcState;

#define TYPE_VEXRISCV_INTC "riscv.vexriscv.intc"

DECLARE_INSTANCE_CHECKER(VexRiscvIntcState, VEXRISCV_INTC, TYPE_VEXRISCV_INTC)

DeviceState *vexriscv_intc_create(void);

#define CSR_M_INTC_ENABLE  0xBC0
#define CSR_M_INTC_PENDING 0xFC0
#define CSR_S_INTC_ENABLE  0x9C0
#define CSR_S_INTC_PENDING 0xDC0

#endif
