// litex-uart.c - simple uart compatible with the one in litex
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
#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "hw/irq.h"
#include "hw/char/litex-uart.h"
#include "hw/qdev-properties-system.h"

// The actual hw has some quirky behaviour...
#define BUG_COMPAT 1

#ifdef BUG_COMPAT
// litex's implementation has the pending bits *after* the enable mask
inline static int is_irq_pending(LitexUartState *s) {
	return s->pending;
}
inline static void set_irq_pending(LitexUartState *s, uint32_t bit) {
	if (s->enable & bit) {
		qatomic_or(&s->pending, bit);
	}
}
#else
inline static int is_irq_pending(LitexUartState *s) {
	return s->pending & s->enable;
}
inline static void set_irq_pending(LitexUartState *s, uint32_t bit) {
	qatomic_or(&s->pending, bit);
}
#endif


static void litex_uart_update_irq(LitexUartState *s) {
	if (is_irq_pending(s)) {
		qemu_irq_raise(s->irq);
	} else {
		qemu_irq_lower(s->irq);
	}
}

static uint64_t litex_uart_read(void *opaque, hwaddr addr,
                            unsigned int sz) {
	LitexUartState *s = opaque;
	switch (addr) {
	case LX_UART_RXTX: {
		if (s->status & LX_UART_EV_BIT_RX) {
			uint32_t ch = s->rx;
			qatomic_and(&s->status, ~LX_UART_EV_BIT_RX);
			litex_uart_update_irq(s);
			return ch;
		}
#if BUG_COMPAT
		// litex's implementation returns the last character
		// read on fifo empty
		return s->rx;
#endif
		break;
	}
	case LX_UART_TXFULL:
		return !(s->status & LX_UART_EV_BIT_TX);
	case LX_UART_RXEMPTY:
#if BUG_COMPAT
		// litex's implementation tracks pending rx ready
		// instead of the actual rx ready
		return !(s->pending & LX_UART_EV_BIT_RX);
#else
		return !(s->status & LX_UART_EV_BIT_RX);
#endif
	case LX_UART_EV_STATUS:
		return s->status;
	case LX_UART_EV_PENDING:
		return s->pending;
	case LX_UART_EV_ENABLE:
		return s->enable;
	case LX_UART_TXEMPTY:
		return !!(s->status & LX_UART_EV_BIT_TX);
	case LX_UART_RXFULL:
		return !!(s->status & LX_UART_EV_BIT_RX);
	}

	return 0;
}

static void litex_uart_write(void *opaque, hwaddr addr,
                             uint64_t val, unsigned int sz) {
	LitexUartState *s = opaque;
	switch (addr) {
	case LX_UART_RXTX: {
		unsigned char ch = val;
		// todo: use non-blocking writes
		qemu_chr_fe_write(&s->chr, &ch, 1);
		set_irq_pending(s, LX_UART_EV_BIT_TX);
		litex_uart_update_irq(s);
		break;
	}
	case LX_UART_EV_ENABLE:
		val &= LX_UART_EV_BIT_ALL;
		s->enable = val;
#if BUG_COMPAT
		// enabled IRQs pending immediately tracks status
		qatomic_or(&s->pending, s->status & val);
		// disabled IRQs pending immediately clears pending bits
		qatomic_and(&s->pending, val);
#endif
		litex_uart_update_irq(s);
		break;
	case LX_UART_EV_PENDING: // write to clear
#if BUG_COMPAT
		// enabled IRQs remain pending while status active
		val &= ~(s->enable & s->status);
#endif
		qatomic_and(&s->pending, ~val);
		litex_uart_update_irq(s);
		break;
	}
}

static void litex_uart_do_rx(void *opaque, const uint8_t *buf, int size) {
	LitexUartState *s = opaque;
	s->rx = buf[0];
	qatomic_or(&s->status, LX_UART_EV_BIT_RX);
	set_irq_pending(s, LX_UART_EV_BIT_RX);
	litex_uart_update_irq(s);
}

static int litex_uart_chk_rx(void *opaque) {
	LitexUartState *s = opaque;
	return (s->status & LX_UART_EV_BIT_RX) == 0;
}

static int litex_uart_be_change(void *opaque) {
	LitexUartState *s = opaque;
	qemu_chr_fe_set_handlers(&s->chr, litex_uart_chk_rx, litex_uart_do_rx,
	                         NULL, litex_uart_be_change, s, NULL, true);
	return 0;
}

static void litex_uart_realize(DeviceState *dev, Error **error) {
	LitexUartState *s = LITEX_UART(dev);

	qemu_chr_fe_set_handlers(&s->chr, litex_uart_chk_rx, litex_uart_do_rx,
	                         NULL, litex_uart_be_change, s, NULL, true);
}

static void litex_uart_reset(DeviceState *dev) {
	LitexUartState *s = LITEX_UART(dev);
	s->status = LX_UART_EV_BIT_TX;
	s->pending = 0;
	s->enable = 0;
	qemu_irq_lower(s->irq);
}

static const MemoryRegionOps litex_uart_ops = {
	.read = litex_uart_read,
	.write = litex_uart_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.valid = {
		.min_access_size = 1,
		.max_access_size = 4,
	},
};

static void litex_uart_init(Object *obj) {
	SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
	LitexUartState *s = LITEX_UART(obj);
	memory_region_init_io(&s->mmio, OBJECT(s), &litex_uart_ops, s,
	                      TYPE_LITEX_UART, LX_UART_MAX);
	sysbus_init_mmio(sbd, &s->mmio);
	sysbus_init_irq(sbd, &s->irq);
}

static Property litex_uart_props[] = {
	DEFINE_PROP_CHR("chardev", LitexUartState, chr),
	DEFINE_PROP_END_OF_LIST(),
};

static void litex_uart_class_init(ObjectClass *oc, void* data) {
	DeviceClass *dc = DEVICE_CLASS(oc);

	dc->realize = litex_uart_realize;
	dc->reset = litex_uart_reset;

	device_class_set_props(dc, litex_uart_props);
	set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
}

static const TypeInfo litex_uart_info = {
	.name = TYPE_LITEX_UART,
	.parent = TYPE_SYS_BUS_DEVICE,
	.class_init = litex_uart_class_init,
	.instance_init = litex_uart_init,
	.instance_size = sizeof(LitexUartState),
};

static void litex_uart_register_types(void) {
	type_register_static(&litex_uart_info);
}


type_init(litex_uart_register_types);

LitexUartState* litex_uart_create(MemoryRegion* mr, hwaddr base, Chardev *chr, qemu_irq irq) {
	DeviceState *dev = qdev_new(TYPE_LITEX_UART);
	SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

	qdev_prop_set_chr(dev, "chardev", chr);
	sysbus_realize_and_unref(sbd, &error_fatal);
	memory_region_add_subregion(mr, base, sysbus_mmio_get_region(sbd, 0));
	sysbus_connect_irq(sbd, 0, irq);

	return LITEX_UART(dev);
}
