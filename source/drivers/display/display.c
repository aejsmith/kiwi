/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <drivers/display.h>

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>
#include <module.h>

/** Display device directory. */
static device_t *display_device_dir;

/** Next device ID. */
static atomic_t display_next_id = 0;

/** Device used as the kernel framebuffer console. */
static display_device_t *display_console_device = NULL;

/** Reset the framebuffer console upon KDBG/fatal entry.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void display_console_register(void *arg1, void *arg2, void *arg3) {
	/* Use the inhibited flag to determine whether a reset is required. */
	if(fb_console.inhibited) {
		fb_console_reset();
		fb_console.inhibited = false;
	}
}

/** Unregister the KDBG/fatal framebuffer console.
 * @param arg1		First notifier argument.
 * @param arg2		Second notifier argument.
 * @param arg3		Third notifier argument. */
static void display_console_unregister(void *arg1, void *arg2, void *arg3) {
	fb_console.inhibited = true;
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
			if(depth && device->modes[i].depth == depth) {
				return &device->modes[i];
			} else if(!ret || device->modes[i].depth > ret->depth) {
				ret = &device->modes[i];
			}
		}
	}

	return ret;
}

/** Open a display device.
 * @param _dev		Device being opened.
 * @param datap		Where to store handle-specific data pointer.
 * @return		0 on success, negative error code on failure. */
static int display_device_open(device_t *_dev, void **datap) {
	display_device_t *device = _dev->data;

	if(!atomic_cmp_set(&device->open, 0, 1)) {
		return -ERR_IN_USE;
	}

	return 0;
}

/** Close a display device.
 * @param _dev		Device being closed.
 * @param data		Unused.
 * @return		0 on success, negative error code on failure. */
static void display_device_close(device_t *_dev, void *data) {
	display_device_t *device = _dev->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);
}

/** Signal that a display device event is being waited for.
 * @param _dev		Device to wait for.
 * @param data		Unused.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int display_device_wait(device_t *_dev, void *data, object_wait_t *wait) {
	display_device_t *device = _dev->data;

	switch(wait->event) {
	case DISPLAY_EVENT_REDRAW:
		if(device->redraw) {
			device->redraw = false;
			object_wait_callback(wait);
		} else {
			notifier_register(&device->redraw_notifier, object_wait_notifier, wait);
		}
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Stop waiting for a display device event.
 * @param _dev		Device to stop waiting for.
 * @param data		Unused.
 * @param wait		Wait information structure. */
static void display_device_unwait(device_t *_dev, void *data, object_wait_t *wait) {
	display_device_t *device = _dev->data;

	switch(wait->event) {
	case DISPLAY_EVENT_REDRAW:
		notifier_unregister(&device->redraw_notifier, object_wait_notifier, wait);
		break;
	}
}

/** Fault handler for memory regions mapping a display device.
 * @param _dev		Device fault occurred on.
 * @param data		Unused.
 * @param offset	Offset into device fault occurred at (page aligned).
 * @param physp		Where to store address of page to map.
 * @return		0 on success, negative error code on failure. */
static int display_device_fault(device_t *_dev, void *data, offset_t offset, phys_ptr_t *physp) {
	display_device_t *device = _dev->data;

	return device->ops->fault(device, offset, physp);
}

/** Handler for display device requests.
 * @param _dev		Device request is being made on.
 * @param data		Unused.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		0 value on success, negative error code on failure. */
static int display_device_request(device_t *_dev, void *data, int request, void *in,
                                  size_t insz, void **outp, size_t *outszp) {
	display_device_t *device = _dev->data;
	display_mode_t *mode = NULL;
	phys_ptr_t phys;
	uint16_t id;
	int ret;

	switch(request) {
	case DISPLAY_MODE_COUNT:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}
		*outp = kmemdup(&device->count, sizeof(size_t), MM_SLEEP);
		*outszp = sizeof(size_t);
		return 0;
	case DISPLAY_GET_MODES:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}
		*outp = kmemdup(device->modes, sizeof(display_mode_t) * device->count, MM_SLEEP);
		*outszp = sizeof(display_mode_t) * device->count;
		return 0;
	case DISPLAY_GET_PREFERRED_MODE:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}

		mutex_lock(&device->lock);

		/* For now just return whatever mode the kernel console is
		 * using, and fallback on 1024x768, then 800x600 if the mode
		 * is unavailable. */
		if(!(mode = display_mode_find(device, fb_console_width, fb_console_height, fb_console_depth))) {
			if(!(mode = display_mode_find(device, 1024, 768, 0))) {
				if(!(mode = display_mode_find(device, 800, 600, 0))) {
					mutex_unlock(&device->lock);
					return -ERR_NOT_FOUND;
				}
			}
		}

		*outp = kmemdup(mode, sizeof(display_mode_t), MM_SLEEP);
		*outszp = sizeof(display_mode_t);
		mutex_unlock(&device->lock);
		return 0;
	case DISPLAY_SET_MODE:
		if(in && insz != sizeof(uint16_t)) {
			return -ERR_PARAM_INVAL;
		} else if(!device->ops->mode_set) {
			return -ERR_NOT_SUPPORTED;
		}

		mutex_lock(&device->lock);

		if(!in) {
			if((ret = device->ops->mode_set(device, NULL)) != 0) {
				mutex_unlock(&device->lock);
				return ret;
			}

			if(device == display_console_device) {
				notifier_unregister(&fatal_notifier, display_console_register, NULL);
				notifier_unregister(&kdbg_entry_notifier, display_console_register, NULL);
				notifier_unregister(&kdbg_exit_notifier, display_console_unregister, NULL);
				display_console_device = NULL;
				fb_console.inhibited = false;
			}
			device->curr_mode = NULL;
		} else {
			/* Look for the mode requested. */
			id = *(uint16_t *)in;
			if(!(mode = display_mode_get(device, id))) {
				mutex_unlock(&device->lock);
				return -ERR_NOT_FOUND;
			}

			if((ret = device->ops->mode_set(device, mode)) != 0) {
				mutex_unlock(&device->lock);
				return ret;
			}

			device->curr_mode = mode;

			/* Set this device as the kernel console if there
			 * isn't one. */
			if(!display_console_device || display_console_device == device) {
				if((ret = device->ops->fault(device, mode->offset, &phys)) != 0) {
					fatal("Could not get video device framebuffer (%d)", ret);
				}

				/* Point the framebuffer console at the device. */
				fb_console_reconfigure(mode->width, mode->height, mode->depth, phys);
				fb_console.inhibited = true;

				/* Register notifiers to reset the console upon
				 * KDBG/fatal. */
				if(!display_console_device) {
					notifier_register(&fatal_notifier, display_console_register, device);
					notifier_register(&kdbg_entry_notifier, display_console_register, device);
					notifier_register(&kdbg_exit_notifier, display_console_unregister, device);
				}

				display_console_device = device;
			}
		}

		mutex_unlock(&device->lock);
		return 0;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->ops->request) {
			mutex_lock(&device->lock);
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
	.open = display_device_open,
	.close = display_device_close,
	.wait = display_device_wait,
	.unwait = display_device_unwait,
	.fault = display_device_fault,
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
 * @return		0 on success, negative error code on failure. */
int display_device_create(const char *name, device_t *parent, display_ops_t *ops,
                          void *data, display_mode_t *modes, size_t count,
                          display_device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "display" } },
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
	notifier_init(&device->redraw_notifier, device);
	device->id = atomic_inc(&display_next_id);
	device->ops = ops;
	device->data = data;
	device->modes = kmemdup(modes, sizeof(display_mode_t) * count, MM_SLEEP);
	device->count = count;
	device->curr_mode = NULL;
	device->redraw = false;

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
			fatal("Could not create device alias (%d)", ret);
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
 * @param device	Device to remove.
 * @return		0 on success, negative error code on failure. */
int display_device_destroy(display_device_t *device) {
	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(display_device_destroy);

/** Initialisation function for the display module.
 * @return		0 on success, negative error code on failure. */
static int display_init(void) {
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
