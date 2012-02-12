/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Display device interface.
 *
 * @note		At the moment, this is nowhere near a proper display
 *			device interface...
 */

#include <drivers/display.h>

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

/** Display device directory. */
static device_t *display_device_dir;

/** Next device ID. */
static atomic_t next_display_id = 0;

/** Device used as the kernel framebuffer console. */
static display_device_t *display_console_device = NULL;

/** Convert a mode's pixel format to a depth in bits.
 * @param mode		Mode to get depth of.
 * @return		Depth in bits. */
static uint16_t display_mode_depth(const display_mode_t *mode) {
	switch(mode->format) {
	case PIXEL_FORMAT_ARGB32:
	case PIXEL_FORMAT_BGRA32:
	case PIXEL_FORMAT_RGB32:
	case PIXEL_FORMAT_BGR32:
		return 32;
	case PIXEL_FORMAT_RGB24:
	case PIXEL_FORMAT_BGR24:
		return 24;
	case PIXEL_FORMAT_ARGB16:
	case PIXEL_FORMAT_BGRA16:
	case PIXEL_FORMAT_RGB16:
	case PIXEL_FORMAT_BGR16:
		return 16;
	case PIXEL_FORMAT_RGB15:
	case PIXEL_FORMAT_BGR15:
		return 15;
	case PIXEL_FORMAT_IDX8:
	case PIXEL_FORMAT_GREY8:
		return 8;
	}
	return 0;
}

/** Trigger a redraw event after KDB has run.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void display_console_redraw(void *arg1, void *arg2, void *arg3) {
	display_console_device->redraw = true;
	notifier_run_unlocked(&display_console_device->redraw_notifier, NULL, false);
}

/** Get a display mode by ID.
 * @param device	Device to get from.
 * @param id		ID of mode to get.
 * @return		Pointer to mode if found, NULL if not. */
static display_mode_t *display_mode_get(display_device_t *device, uint16_t id) {
	size_t i;

	for(i = 0; i < device->count; i++) {
		if(device->modes[i].id == id) {
			return &device->modes[i];
		}
	}

	return NULL;
}

/** Find a display mode.
 * @param device	Device to find in.
 * @param width		Width of mode.
 * @param height	Height of mode.
 * @param depth		Depth of mode. If 0, matching mode with highest depth
 *			will be returned.
 * @return		Pointer to mode if found, NULL if not. */
static display_mode_t *display_mode_find(display_device_t *device, uint16_t width,
                                         uint16_t height, uint8_t depth) {
	display_mode_t *ret = NULL;
	size_t i;

	for(i = 0; i < device->count; i++) {
		if(device->modes[i].width == width && device->modes[i].height == height) {
			if(depth && display_mode_depth(&device->modes[i]) == depth) {
				return &device->modes[i];
			} else if(!ret || display_mode_depth(&device->modes[i]) > display_mode_depth(ret)) {
				ret = &device->modes[i];
			}
		}
	}

	return ret;
}

/** Destroy a display device.
 * @param _device	Device to destroy. */
static void display_device_destroy(device_t *_device) {
	/* TODO: I'm lazy. */
	kprintf(LOG_WARN, "display: destroy is not implemented, happily leaking a bunch of memory!\n");
}

/** Open a display device.
 * @param _device	Device being opened.
 * @param datap		Where to store handle-specific data pointer (Unused).
 * @return		Status code describing result of the operation. */
static status_t display_device_open(device_t *_device, void **datap) {
	display_device_t *device = _device->data;

	if(!atomic_cas(&device->open, 0, 1)) {
		return STATUS_IN_USE;
	}

	/* If this is the kernel console device, register the redraw notifier. */
	if(device == display_console_device) {
		fb_console_control(FB_CONSOLE_ACQUIRE, NULL);
		notifier_register(&kdb_exit_notifier, display_console_redraw, device);
	}
	return STATUS_SUCCESS;
}

/** Close a display device.
 * @param _device	Device being closed.
 * @param data		Unused.
 * @return		Status code describing result of the operation. */
static void display_device_close(device_t *_device, void *data) {
	display_device_t *device = _device->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);

	if(device == display_console_device) {
		notifier_unregister(&kdb_exit_notifier, display_console_redraw, device);
		fb_console_control(FB_CONSOLE_RELEASE, NULL);
	}
}

/** Signal that a display device event is being waited for.
 * @param _device	Device to wait for.
 * @param data		Unused.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer.
 * @return		Status code describing result of the operation. */
static status_t display_device_wait(device_t *_device, void *data, int event, void *sync) {
	display_device_t *device = _device->data;

	switch(event) {
	case DISPLAY_EVENT_REDRAW:
		if(device->redraw) {
			device->redraw = false;
			object_wait_signal(sync);
		} else {
			notifier_register(&device->redraw_notifier, object_wait_notifier, sync);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a display device event.
 * @param _device	Device to stop waiting for.
 * @param data		Unused.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer. */
static void display_device_unwait(device_t *_device, void *data, int event, void *sync) {
	display_device_t *device = _device->data;

	switch(event) {
	case DISPLAY_EVENT_REDRAW:
		notifier_unregister(&device->redraw_notifier, object_wait_notifier, sync);
		break;
	}
}

/** Get a page for the device.
 * @param _device	Device to get page from.
 * @param data		Unused.
 * @param offset	Offset into device of page to get.
 * @param physp		Where to store address of page.
 * @return		Status code describing result of the operation. */
static status_t display_device_get_page(device_t *_device, void *data, offset_t offset, phys_ptr_t *physp) {
	display_device_t *device = _device->data;

	if(offset >= (offset_t)device->mem_size) {
		return STATUS_NOT_FOUND;
	}

	*physp = device->mem_phys + offset;
	return STATUS_SUCCESS;
}

/** Convert a display_mode_t to an fb_info_t.
 * @param device	Device the mode is from.
 * @param mode		Mode to convert.
 * @param info		Information structure to fill in. */
static void display_mode_to_fb_info(display_device_t *device, const display_mode_t *mode, fb_info_t *info) {
	info->width = mode->width;
	info->height = mode->height;
	info->depth = display_mode_depth(mode);
	info->bytes_per_pixel = ROUND_UP(info->depth, 8) / 8;
	info->addr = device->mem_phys + mode->offset;

	switch(mode->format) {
	case PIXEL_FORMAT_ARGB32:
	case PIXEL_FORMAT_RGB32:
	case PIXEL_FORMAT_RGB24:
		info->red_position = 16;
		info->red_size = 8;
		info->green_position = 8;
		info->green_size = 8;
		info->blue_position = 0;
		info->blue_size = 8;
		break;
	case PIXEL_FORMAT_ARGB16:
	case PIXEL_FORMAT_RGB16:
		info->red_position = 11;
		info->red_size = 5;
		info->green_position = 5;
		info->green_size = 6;
		info->blue_position = 0;
		info->blue_size = 5;
		break;
	case PIXEL_FORMAT_RGB15:
		info->red_position = 10;
		info->red_size = 5;
		info->green_position = 5;
		info->green_size = 5;
		info->blue_position = 0;
		info->blue_size = 5;
		break;
	default:
		fatal("TODO");
	}
}

/** Handler for display device requests.
 * @param _device	Device request is being made on.
 * @param data		Unused.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t display_device_request(device_t *_device, void *data, int request, const void *in,
                                       size_t insz, void **outp, size_t *outszp) {
	display_device_t *device = _device->data;
	display_mode_t *mode = NULL;
	fb_info_t info;
	status_t ret;
	uint16_t id;

	switch(request) {
	case DISPLAY_MODE_COUNT:
		if(!outp || !outszp) {
			return STATUS_INVALID_ARG;
		}
		*outp = kmemdup(&device->count, sizeof(size_t), MM_WAIT);
		*outszp = sizeof(size_t);
		return STATUS_SUCCESS;
	case DISPLAY_GET_MODES:
		if(!outp || !outszp) {
			return STATUS_INVALID_ARG;
		}
		*outp = kmemdup(device->modes, sizeof(display_mode_t) * device->count, MM_WAIT);
		*outszp = sizeof(display_mode_t) * device->count;
		return STATUS_SUCCESS;
	case DISPLAY_GET_PREFERRED_MODE:
		if(!outp || !outszp) {
			return STATUS_INVALID_ARG;
		}

		mutex_lock(&device->lock);

		/* For now just return whatever mode the kernel console is
		 * using, and fallback on 1024x768, then 800x600 if the mode
		 * is unavailable. */
		fb_console_control(FB_CONSOLE_INFO, &info);
		mode = display_mode_find(device, info.width, info.height, info.depth);
		if(!mode) {
			mode = display_mode_find(device, 1024, 768, 0);
			if(!mode) {
				mode = display_mode_find(device, 800, 600, 0);
				if(!mode) {
					mutex_unlock(&device->lock);
					return STATUS_NOT_FOUND;
				}
			}
		}

		*outp = kmemdup(mode, sizeof(display_mode_t), MM_WAIT);
		*outszp = sizeof(display_mode_t);
		mutex_unlock(&device->lock);
		return STATUS_SUCCESS;
	case DISPLAY_SET_MODE:
		if(in && insz != sizeof(uint16_t)) {
			return STATUS_INVALID_ARG;
		} else if(!device->ops->set_mode) {
			return STATUS_NOT_SUPPORTED;
		}

		mutex_lock(&device->lock);

		if(!in) {
			ret = device->ops->set_mode(device, NULL);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&device->lock);
				return ret;
			}

			if(device == display_console_device) {
				notifier_unregister(&kdb_exit_notifier, display_console_redraw, device);
				fb_console_control(FB_CONSOLE_RELEASE, NULL);
				display_console_device = NULL;
			}
			device->curr_mode = NULL;
		} else {
			/* Look for the mode requested. */
			id = *(uint16_t *)in;
			mode = display_mode_get(device, id);
			if(!mode) {
				mutex_unlock(&device->lock);
				return STATUS_NOT_FOUND;
			}

			ret = device->ops->set_mode(device, mode);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&device->lock);
				return ret;
			}

			device->curr_mode = mode;

			/* Set this device as the kernel console if there
			 * isn't one. */
			if(!display_console_device || display_console_device == device) {
				/* Register a notifier to redraw the console
				 * after KDB has run, and acquire the console
				 * to prevent kernel output. */
				if(!display_console_device) {
					fb_console_control(FB_CONSOLE_ACQUIRE, NULL);
					notifier_register(&kdb_exit_notifier, display_console_redraw, device);
				}

				display_console_device = device;

				/* Point the framebuffer console at the device. */
				display_mode_to_fb_info(device, mode, &info);
				fb_console_control(FB_CONSOLE_CONFIGURE, &info);
			}
		}

		mutex_unlock(&device->lock);
		return STATUS_SUCCESS;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->ops->request) {
			mutex_lock(&device->lock);
			ret = device->ops->request(device, request, in, insz, outp, outszp);
			mutex_unlock(&device->lock);
			return ret;
		} else {
			return STATUS_INVALID_REQUEST;
		}
	}
}

/** Display device operations structure. */
static device_ops_t display_device_ops = {
	.destroy = display_device_destroy,
	.open = display_device_open,
	.close = display_device_close,
	.wait = display_device_wait,
	.unwait = display_device_unwait,
	.get_page = display_device_get_page,
	.request = display_device_request,
};

/** Create a new display device.
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the display device
 *			container.
 * @param ops		Display device operations structure.
 * @param data		Data used by driver.
 * @param modes		Pointer to array of mode structures (will duplicate).
 * @param count		Number of modes.
 * @param devicep	Where to store pointer to device structure.
 * @return		Status code describing result of the operation. */
status_t display_device_create(const char *name, device_t *parent, display_ops_t *ops,
                               void *data, display_mode_t *modes, size_t count,
                               phys_ptr_t mem_phys, size_t mem_size,
                               device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "display" } },
	};
	char dname[DEVICE_NAME_MAX];
	display_device_t *device;
	status_t ret;

	if((parent && !name) || (name && !parent) || !ops || !modes || !count || !devicep) {
		return STATUS_INVALID_ARG;
	}

	device = kmalloc(sizeof(display_device_t), MM_WAIT);
	mutex_init(&device->lock, "display_device_lock", 0);
	atomic_set(&device->open, 0);
	notifier_init(&device->redraw_notifier, device);
	device->id = atomic_inc(&next_display_id);
	device->ops = ops;
	device->data = data;
	device->curr_mode = NULL;
	device->redraw = false;
	device->modes = kmemdup(modes, sizeof(display_mode_t) * count, MM_WAIT);
	device->count = count;
	device->mem_phys = mem_phys;
	device->mem_size = mem_size;

	/* Create the device tree node. */
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		ret = device_create(name, parent, &display_device_ops, device, attrs,
		                    ARRAY_SIZE(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}

		/* Should not fail - only possible failure is if name already
		 * exists, and ID should be unique. */
		device_alias(dname, display_device_dir, *devicep, NULL);
	} else {
		ret = device_create(dname, display_device_dir, &display_device_ops, device,
		                    attrs, ARRAY_SIZE(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}
	}

	return STATUS_SUCCESS;
}
MODULE_EXPORT(display_device_create);

/** Initialisation function for the display module.
 * @return		Status code describing result of the operation. */
static status_t display_init(void) {
	/* Create the display device directory. */
	return device_create("display", device_tree_root, NULL, NULL, NULL, 0, &display_device_dir);
}

/** Unloading function for the display module.
 * @return		Status code describing result of the operation. */
static status_t display_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("display");
MODULE_DESC("Display device class manager");
MODULE_FUNCS(display_init, display_unload);
