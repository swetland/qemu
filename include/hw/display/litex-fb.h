// litex-fb.h - simple framebuffer
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

#ifndef _HW_DISPLAY_LITEX_FB_H
#define _HW_DISPLAY_LITEX_FB_H

#include "ui/console.h"
#include "hw/sysbus.h"
#include "hw/display/framebuffer.h"

typedef struct {
	SysBusDevice parent;

	MemoryRegion vram;
	QemuConsole *con;

	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t stride;
	uint32_t offset;
	uint32_t size;

	int new_mode;
} LitexFrameBufferState;

#define TYPE_LITEX_FB "riscv.litex.fb"
#define LITEX_FB(obj) OBJECT_CHECK(LitexFrameBufferState, (obj), TYPE_LITEX_FB)

LitexFrameBufferState* litex_fb_create(MemoryRegion* mr, hwaddr base);

#endif
