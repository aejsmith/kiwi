/* Kiwi display device interface
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Display device interface.
 *
 * @note		At the moment, this is nowhere near a proper display
 *			device interface...
 */

#include <console/console.h>

#include <drivers/display.h>

#include <lib/utility.h>
#include <lib/string.h>

#include <mm/malloc.h>

#include <types/atomic.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>
#include <module.h>

/** Get red colour from RGB value. */
#define RED(x, bits)		((x >> (24 - bits)) & ((1 << bits) - 1))

/** Get green colour from RGB value. */
#define GREEN(x, bits)		((x >> (16 - bits)) & ((1 << bits) - 1))

/** Get blue colour from RGB value. */
#define BLUE(x, bits)		((x >> (8  - bits)) & ((1 << bits) - 1))

/** Display device directory. */
static device_t *display_device_dir;

/** Next device ID. */
static atomic_t display_next_id = 0;

/** Device used as the KDBG/fatal console. */
static display_device_t *display_console_device = NULL;
static int display_console_x = 0;
static int display_console_y = 0;
static int display_console_cols = 0;
static int display_console_rows = 0;

/** Draw a pixel on a 16-bit (5:6:5) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void display_console_putpixel565(uint16_t *dest, uint32_t colour) {
	*dest = (RED(colour, 5) << 11) | (GREEN(colour, 6) << 5) | BLUE(colour, 5);
}

/** Draw a pixel on a 24-bit (8:8:8) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void display_console_putpixel888(uint8_t *dest, uint32_t colour) {
	dest[2] = RED(colour, 8);
	dest[1] = GREEN(colour, 8);
	dest[0] = BLUE(colour, 8);
}

/** Draw a pixel on a 32-bit (0:8:8:8) framebuffer.
 * @param dest		Destination location for pixel.
 * @param colour	RGB colour value to draw in. */
static inline void display_console_putpixel0888(uint32_t *dest, uint32_t colour) {
	*dest = colour;
}

/** Draw a pixel on a framebuffer.
 * @param device	Device to draw on.
 * @param colour	Colour to draw in.
 * @param x		X position at which to place pixel.
 * @param y		Y position at which to place pixel. */
static void display_console_putpixel(display_device_t *device, uint32_t colour, uint32_t x, uint32_t y) {
	void *dest = device->fb + (((y * device->curr_mode->width) + x) * (device->curr_mode->bpp / 8));

	switch(device->curr_mode->bpp) {
	case 16:
		display_console_putpixel565(dest, colour);
		break;
	case 24:
		display_console_putpixel888(dest, colour);
		break;
	case 32:
		display_console_putpixel0888(dest, colour);
		break;
	}
}


/** Write a character to the display device console.
 * @param ch		Character to print. */
static void display_console_putch(unsigned char ch) {
	display_device_t *device = display_console_device;
	int x, y, i, j;
	size_t row;

	row = (device->curr_mode->width * FONT_HEIGHT) * (device->curr_mode->bpp / 8);

	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(display_console_x) {
			display_console_x--;
		} else if(display_console_y) {
			display_console_x = display_console_cols - 1;
			display_console_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		display_console_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		display_console_x = 0;
		display_console_y++;
		memset(device->fb + (row * display_console_y), 0, row);
		break;
	case '\t':
		display_console_x += 8 - (display_console_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ') {
			break;
		}

		x = display_console_x * FONT_WIDTH;
		y = display_console_y * FONT_HEIGHT;

		for(i = 0; i < FONT_HEIGHT; i++) {
			for(j = 0; j < FONT_WIDTH; j++) {
				if(FONT_DATA[(ch * FONT_HEIGHT) + i] & (1<<(7-j))) {
					display_console_putpixel(device, 0xffffff, x + j, y + i);
				} else {
					display_console_putpixel(device, 0x0, x + j, y + i);
				}
			}
		}

		display_console_x++;
		break;
	}

	/* If we have reached the edge of the screen insert a new line. */
	if(display_console_x >= display_console_cols) {
		display_console_x = 0;
		display_console_y++;
	}

	/* If we have reached the bottom of the screen, scroll. */
	if(display_console_y >= display_console_rows) {
		memcpy(device->fb, (device->fb + row), row * (display_console_rows - 1));

		/* Fill the last row with blanks. */
		memset(device->fb + (row * (display_console_rows - 1)), 0, row);
		display_console_y = display_console_rows - 1;
	}
}

/** Display device console operations structure. */
static console_t display_console = {
	.putch = display_console_putch,
};

/** Register the KDBG/fatal framebuffer console.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param _dev		Device pointer. */
static void display_console_register(void *arg1, void *arg2, void *_dev) {
	display_device_t *device = display_console_device;

	if(list_empty(&display_console.header)) {
		display_console_x = 0;
		display_console_y = 0;
		display_console_rows = device->curr_mode->height / FONT_HEIGHT;
		display_console_cols = device->curr_mode->width / FONT_WIDTH;

		memset(device->fb, 0, (device->curr_mode->width * FONT_HEIGHT) * (device->curr_mode->bpp / 8));

		console_register(&display_console);
	}
}

/** Unregister the KDBG/fatal framebuffer console.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param _dev		Device pointer. */
static void display_console_unregister(void *arg1, void *arg2, void *_dev) {
	console_unregister(&display_console);
	condvar_broadcast(&display_console_device->redraw);
}

/** Open a display device.
 * @param _dev		Device being opened.
 * @return		0 on success, negative error code on failure. */
static int display_device_get(device_t *_dev) {
	display_device_t *device = _dev->data;

	if(!atomic_cmp_set(&device->open, 0, 1)) {
		return -ERR_IN_USE;
	}

	return 0;
}

/** Close a display device.
 * @param _dev		Device being closed.
 * @return		0 on success, negative error code on failure. */
static void display_device_release(device_t *_dev) {
	display_device_t *device = _dev->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);
}

/** Fault handler for memory regions mapping a display device.
 * @param _dev		Device fault occurred on.
 * @param offset	Offset into device fault occurred at (page aligned).
 * @param physp		Where to store address of page to map.
 * @return		0 on success, negative error code on failure. */
static int display_device_fault(device_t *_dev, offset_t offset, phys_ptr_t *physp) {
	display_device_t *device = _dev->data;

	return device->ops->fault(device, offset, physp);
}

/** Handler for display device requests.
 * @param _dev		Device request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		0 value on success, negative error code on failure. */
static int display_device_request(device_t *_dev, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	display_device_t *device = _dev->data;
	display_mode_t *mode = NULL;
	identifier_t id;
	phys_ptr_t phys;
	size_t i;
	int ret;

	switch(request) {
	case DISPLAY_MODE_COUNT:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}
		*outp = kmemdup(&device->count, sizeof(size_t), MM_SLEEP);
		*outszp = sizeof(size_t);
		return 0;
	case DISPLAY_MODE_GET:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}
		*outp = kmemdup(device->modes, sizeof(display_mode_t) * device->count, MM_SLEEP);
		*outszp = sizeof(display_mode_t) * device->count;
		return 0;
	case DISPLAY_MODE_SET:
		if(in && insz != sizeof(identifier_t)) {
			return -ERR_PARAM_INVAL;
		} else if(!device->ops->mode_set) {
			return -ERR_NOT_SUPPORTED;
		}

		mutex_lock(&device->lock, 0);

		if(!in) {
			if((ret = device->ops->mode_set(device, NULL)) != 0) {
				mutex_unlock(&device->lock);
				return ret;
			}

			if(device == display_console_device) {
				page_phys_unmap(device->fb, device->fb_size, true);

				notifier_unregister(&fatal_notifier, display_console_register, device);
				notifier_unregister(&kdbg_entry_notifier, display_console_register, device);
				notifier_unregister(&kdbg_exit_notifier, display_console_unregister, device);

				device->fb = NULL;
				display_console_device = NULL;
			}
			device->curr_mode = NULL;
		}

		/* Look for the mode requested. */
		id = *(identifier_t *)in;
		for(i = 0; i < device->count; i++) {
			if(device->modes[i].id == id) {
				mode = &device->modes[i];
				break;
			}
		}
		if(!mode) {
			mutex_unlock(&device->lock);
			return -ERR_NOT_FOUND;
		}

		if((ret = device->ops->mode_set(device, mode)) != 0) {
			mutex_unlock(&device->lock);
			return ret;
		}

		device->curr_mode = mode;

		/* Set this device as the KDBG/fatal console if there isn't
		 * already one set up. */
		if(!display_console_device || display_console_device == device) {
			if((ret = device->ops->fault(device, mode->offset, &phys)) != 0) {
				fatal("Could not get video device framebuffer");
			}

			/* Unmap old framebuffer. */
			if(device->fb) {
				page_phys_unmap(device->fb, device->fb_size, true);
			}

			device->fb_size = mode->width * mode->height * (mode->bpp / 8);
			device->fb = page_phys_map(phys, device->fb_size, MM_SLEEP);

			if(!display_console_device) {
				notifier_register(&fatal_notifier, display_console_register, device);
				notifier_register(&kdbg_entry_notifier, display_console_register, device);
				notifier_register(&kdbg_exit_notifier, display_console_unregister, device);
			}

			display_console_device = device;
		}

		mutex_unlock(&device->lock);
		return 0;
	case DISPLAY_REDRAW_WAIT:
		/* FIXME: This should be done by a handle event or something
		 * after I've implemented handle waiting. */
		mutex_lock(&device->lock, 0);
		condvar_wait(&device->redraw, &device->lock, NULL, 0);
		mutex_unlock(&device->lock);
		return 0;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->ops->request) {
			mutex_lock(&device->lock, 0);
			ret = device->ops->request(device, request, in, insz, outp, outszp);
			mutex_unlock(&device->lock);
			return ret;
		} else {
			return -ERR_PARAM_INVAL;
		}
	}
}

/** Display device operations structure. */
static device_ops_t display_device_ops = {
	.get = display_device_get,
	.release = display_device_release,
	.fault = display_device_fault,
	.request = display_device_request,
};

/** Create a new display device.
 *
 * Registers a new display device with the display device manager.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the display device
 *			container.
 * @param ops		Display device operations structure.
 * @param data		Data used by driver.
 * @param modes		Pointer to array of mode structures (will duplicate).
 * @param count		Number of modes.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int display_device_create(const char *name, device_t *parent, display_ops_t *ops,
                          void *data, display_mode_t *modes, size_t count,
                          display_device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, {string: "display"} },
	};
	char dname[DEVICE_NAME_MAX];
	display_device_t *device;
	int ret;

	if((parent && !name) || (name && !parent) || !ops || !modes || !count || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	device = kmalloc(sizeof(display_device_t), MM_SLEEP);
	mutex_init(&device->lock, "display_device_lock", 0);
	atomic_set(&device->open, 0);
	condvar_init(&device->redraw, "display_redraw");
	device->id = atomic_inc(&display_next_id);
	device->ops = ops;
	device->data = data;
	device->modes = kmemdup(modes, sizeof(display_mode_t) * count, MM_SLEEP);
	device->count = count;
	device->curr_mode = NULL;
	device->fb = NULL;
	device->fb_size = 0;

	/* Create the device tree node. */
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		if((ret = device_create(name, parent, &display_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		} else if((ret = device_alias(dname, display_device_dir, device->device, &device->alias)) != 0) {
			/* Should not fail - only possible failure is if name
			 * already exists, and ID should be unique. Note that
			 * with current ID allocation implementation this can
			 * happen - FIXME. */
			fatal("Could not create device alias");
		}
	} else {
		if((ret = device_create(dname, display_device_dir, &display_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		}
		device->alias = NULL;
	}

	*devicep = device;
	return 0;
}
MODULE_EXPORT(display_device_create);

/** Destroy a display device.
 *
 * Removes a display device from the device tree.
 *
 * @param device	Device to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int display_device_destroy(display_device_t *device) {
	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(display_device_destroy);

/** Initialization function for the display module.
 * @return		0 on success, negative error code on failure. */
static int display_init(void) {
	list_init(&display_console.header);

	/* Create the display device directory. */
	return device_create("display", device_tree_root, NULL, NULL, NULL, 0, &display_device_dir);
}

/** Unloading function for the display module.
 * @return		0 on success, negative error code on failure. */
static int display_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("display");
MODULE_DESC("Display device class manager");
MODULE_FUNCS(display_init, display_unload);
