// litex-timer.h - litex compatible timer
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

#ifndef _HW_TIMER_LITEX_TIMER_H
#define _HW_TIMER_LITEX_TIMER_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/ptimer.h"

typedef struct {
	SysBusDevice parent;

	MemoryRegion mmio;
	qemu_irq irq;
	ptimer_state *ptimer;

	uint32_t value;
	uint32_t loadval;
	uint32_t enabled;

	// events
	uint32_t status;
	uint32_t pending;
	uint32_t enable;
} LitexTimerState;

#define TYPE_LITEX_TIMER "riscv.litex.timer"

#define LITEX_TIMER(obj) OBJECT_CHECK(LitexTimerState, (obj), TYPE_LITEX_TIMER)


// Litex Timer Registers

#define LX_TIMER_LOAD         0x000 // write to set value
#define LX_TIMER_RELOAD       0x004 // value becomes this on underflow
#define LX_TIMER_EN           0x008 // write 1 to start 0 to stop
#define LX_TIMER_UPDATE_VALUE 0x00C // write 1 to latch value for reading
#define LX_TIMER_VALUE        0x010 // ro: last latched value
#define LX_TIMER_EV_STATUS    0x014 // active events
#define LX_TIMER_EV_PENDING   0x018 // pending events (write to clear)
#define LX_TIMER_EV_ENABLE    0x01C // events that cause IRQs when pending
#define LX_TIMER_MAX          0x100

#define LX_TIMER_EVb_ZERO     (1U << 0) // value is zero

LitexTimerState* litex_timer_create(MemoryRegion *mr, hwaddr base, qemu_irq irq);

#endif
