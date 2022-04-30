// litex-uart.h - simple uart compatible with the one in litex
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

#ifndef _HW_CHAR_LITEX_UART_H
#define _HW_CHAR_LITEX_UART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

typedef struct LitexUartState {
	SysBusDevice parent;

	MemoryRegion mmio;
	CharBackend chr;
	qemu_irq irq;

	uint32_t status;
	uint32_t pending;
	uint32_t enable;
	uint32_t rx;
} LitexUartState;

#define TYPE_LITEX_UART "riscv.litex.uart"
#define LITEX_UART(obj) OBJECT_CHECK(LitexUartState, (obj), TYPE_LITEX_UART)


#define LXUART_RXTX        0x000 // r/w to rx/tx
#define LXUART_TXFULL      0x004 // 1 if TX full
#define LXUART_RXEMPTY     0x008 // 1 if RX empty
#define LXUART_EV_STATUS   0x00C // EV BITS current state
#define LXUART_EV_PENDING  0x010 // EV BITS latched state (write to clear)
#define LXUART_EV_ENABLE   0x014 // EV BITS irq enable
#define LXUART_TXEMPTY     0x018 // 1 if TX empty
#define LXUART_RXFULL      0x01C // 1 if RX full
#define LXUART_MAX         0x100

#define LXUART_EV_BIT_TX   (1U << 0)
#define LXUART_EV_BIT_RX   (1U << 1)
#define LXUART_EV_BIT_ALL  0x003

LitexUartState* litex_uart_create(MemoryRegion* mr, hwaddr base, Chardev *chr, qemu_irq irq);

#endif
