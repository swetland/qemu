// litex-fb.c - simple framebuffer
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
#include "ui/pixel_ops.h"
#include "qom/object.h"

#include "hw/display/litex-fb.h"

static void litex_fb_update(void *opaque) {
	LitexFrameBufferState *fbs = LITEX_FB(opaque);
	DisplaySurface *ds = qemu_console_surface(fbs->con);

	if (fbs->new_mode) {
		fbs->new_mode = 0;
		uint8_t *ptr = memory_region_get_ram_ptr(&fbs->vram);
		ds = qemu_create_displaysurface_from(fbs->width, fbs->height,
		                                     fbs->format, fbs->stride,
		                                     ptr + fbs->offset);
		dpy_gfx_replace_surface(fbs->con, ds);
	}

	dpy_gfx_update_full(fbs->con);

	// TODO partial update

	//DirtyBitmapSnapshot *snap = null;
	//snap = memory_region_snapshot_and_clear_dirty(&fbs->vram, fbs->offset,
	//                                              fbs->size, DIRTY_MEMORY_VGA);
}

static const GraphicHwOps litex_fb_ops = {
	//.invalidate = litex_fb_invalidate,
	.gfx_update = litex_fb_update,
};

static void litex_fb_realize(DeviceState *dev, Error **error) {
	LitexFrameBufferState *fbs = LITEX_FB(dev);

	fbs->width = 640;
	fbs->height = 480;
	fbs->stride = fbs->width * 4;
	fbs->format = PIXMAN_LE_x8r8g8b8;
	fbs->offset = 0;
	fbs->size = fbs->stride * fbs->height;
	fbs->new_mode = 1;

	memory_region_init_ram(&fbs->vram, OBJECT(dev), "litex-video-ram",
	                       fbs->width * fbs->height * 4, &error_fatal);
	sysbus_init_mmio(SYS_BUS_DEVICE(dev), &fbs->vram);

	fbs->con = graphic_console_init(dev, 0, &litex_fb_ops, fbs);
	qemu_console_resize(fbs->con, fbs->width, fbs->height);

	memory_region_set_log(&fbs->vram, true, DIRTY_MEMORY_VGA);
}

static void litex_fb_class_init(ObjectClass *oc, void *data) {
	DeviceClass *dc = DEVICE_CLASS(oc);
	set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
	dc->realize = litex_fb_realize;
}

static const TypeInfo litex_fb_info = {
	.name = TYPE_LITEX_FB,
	.parent = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(LitexFrameBufferState),
	.class_init = litex_fb_class_init,
};

static void litex_fb_register_types(void) {
	type_register_static(&litex_fb_info);
}

type_init(litex_fb_register_types)

LitexFrameBufferState* litex_fb_create(MemoryRegion* mr, hwaddr base) {
	DeviceState *dev = qdev_new(TYPE_LITEX_FB);
	SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
	LitexFrameBufferState *fbs = LITEX_FB(dev);
	sysbus_realize_and_unref(sbd, &error_fatal);
	sysbus_mmio_map(sbd, 0, base);
	return fbs;
}
