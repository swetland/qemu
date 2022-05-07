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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/timer/litex-timer.h"
//#include "hw/qdev-properties-system.h"


static void litex_timer_callback(void *opaque) {
	LitexTimerState *ts = opaque;
	ts->pending |= LX_TIMER_EVb_ZERO;
	if (ts->enable & LX_TIMER_EVb_ZERO) {
		qemu_irq_raise(ts->irq);
	}
}

static uint64_t litex_timer_read(void *opaque, hwaddr addr,
                           	 unsigned int sz) {
	LitexTimerState *ts = opaque;
	switch (addr) {
	case LX_TIMER_LOAD:
		return ts->loadval;
	case LX_TIMER_RELOAD:
		return ptimer_get_limit(ts->ptimer);
	case LX_TIMER_EN:
		return ts->enabled;
	case LX_TIMER_VALUE:
		return ts->value;
	case LX_TIMER_EV_STATUS:
		return ts->status;
	case LX_TIMER_EV_PENDING:
		return ts->pending;
	case LX_TIMER_EV_ENABLE:
		return ts->enable;
	}
	return 0;
}

static void litex_timer_write(void *opaque, hwaddr addr,
                       	      uint64_t val, unsigned int sz) {
	LitexTimerState *ts = opaque;
	switch (addr) {
	case LX_TIMER_LOAD:
		ts->loadval = val;
		break;
	case LX_TIMER_RELOAD:
		ptimer_transaction_begin(ts->ptimer);
		ptimer_set_limit(ts->ptimer, val, 0);
		ptimer_transaction_commit(ts->ptimer);
		break;
	case LX_TIMER_EN:
		ptimer_transaction_begin(ts->ptimer);
		if (val) { // start
			ts->enabled = 1;
			if (ts->loadval) { // one-shot
				ptimer_run(ts->ptimer, 1);
				ptimer_set_count(ts->ptimer, ts->loadval);
			} else { // repeating
				ptimer_run(ts->ptimer, 0);
				ptimer_set_count(ts->ptimer, ptimer_get_limit(ts->ptimer));
			}
		} else { // stop
			ts->enabled = 0;
			ptimer_stop(ts->ptimer);
		}
		ptimer_transaction_commit(ts->ptimer);
		break;
	case LX_TIMER_UPDATE_VALUE:
		ts->value = ptimer_get_count(ts->ptimer);
		break;
	case LX_TIMER_EV_PENDING:
		ts->pending &= (~(val & LX_TIMER_EVb_ZERO));
		if (val & LX_TIMER_EVb_ZERO) {
			qemu_irq_lower(ts->irq);
		}
		break;
	case LX_TIMER_EV_ENABLE:
		if (val & LX_TIMER_EVb_ZERO) {
			ts->enable |= LX_TIMER_EVb_ZERO;
			if (ts->pending & LX_TIMER_EVb_ZERO) {
				qemu_irq_raise(ts->irq);
			}
		} else {
			ts->enable &= (~LX_TIMER_EVb_ZERO);
			qemu_irq_lower(ts->irq);
		}
		break;
	}
}

static void litex_timer_realize(DeviceState *dev, Error **error) {
	//LitexTimerState *ts = LITEX_TIMER(dev);
}

static void litex_timer_reset(DeviceState *dev) {
	LitexTimerState *ts = LITEX_TIMER(dev);

	ptimer_transaction_begin(ts->ptimer);
	ptimer_stop(ts->ptimer);
	ptimer_set_freq(ts->ptimer, 50000000);
	ptimer_set_count(ts->ptimer, 0);
	ptimer_set_limit(ts->ptimer, 0, 0);
	ptimer_transaction_commit(ts->ptimer);

	ts->status = 0;
	ts->pending = 0;
	ts->enable = 0;
	ts->value = 0;
}

static const MemoryRegionOps litex_timer_ops = {
	.read = litex_timer_read,
	.write = litex_timer_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.valid = {
		.min_access_size = 1,
		.max_access_size = 4,
	},
};

#define POLICY PTIMER_POLICY_TRIGGER_ONLY_ON_DECREMENT

static void litex_timer_init(Object *obj) {
	SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
	LitexTimerState *ts = LITEX_TIMER(obj);
	memory_region_init_io(&ts->mmio, OBJECT(ts), &litex_timer_ops, ts,
	                      TYPE_LITEX_TIMER, LX_TIMER_MAX);
	sysbus_init_mmio(sbd, &ts->mmio);
	sysbus_init_irq(sbd, &ts->irq);
	ts->ptimer = ptimer_init(litex_timer_callback, ts, POLICY);

	litex_timer_reset(DEVICE(obj));
}

static void litex_timer_class_init(ObjectClass *oc, void* data) {
	DeviceClass *dc = DEVICE_CLASS(oc);
	dc->realize = litex_timer_realize;
	dc->reset = litex_timer_reset;
}

static const TypeInfo litex_timer_info = {
	.name = TYPE_LITEX_TIMER,
	.parent = TYPE_SYS_BUS_DEVICE,
	.class_init = litex_timer_class_init,
	.instance_init = litex_timer_init,
	.instance_size = sizeof(LitexTimerState),
};

static void litex_timer_register_types(void) {
	type_register_static(&litex_timer_info);
}

type_init(litex_timer_register_types)


LitexTimerState* litex_timer_create(MemoryRegion* mr, hwaddr base, qemu_irq irq) {
	DeviceState *dev = qdev_new(TYPE_LITEX_TIMER);
	SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

	sysbus_realize_and_unref(sbd, &error_fatal);
	memory_region_add_subregion(mr, base, sysbus_mmio_get_region(sbd, 0));
	sysbus_connect_irq(sbd, 0, irq);

	return LITEX_TIMER(dev);
}
