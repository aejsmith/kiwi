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
#include <fatal.h>
#include <kdbg.h>
#include <module.h>
#include <status.h>

/** Display device directory. */
static device_t *display_device_dir;

/** Next device ID. */
static atomic_t next_display_id = 0;

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

	if(!atomic_cmp_set(&device->open, 0, 1)) {
		return STATUS_IN_USE;
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
}

/** Signal that a display device event is being waited for.
 * @param _device	Device to wait for.
 * @param data		Unused.
 * @param wait		Wait information structure.
 * @return		Status code describing result of the operation. */
static status_t display_device_wait(device_t *_device, void *data, object_wait_t *wait) {
	display_device_t *device = _device->data;

	switch(wait->event) {
	case DISPLAY_EVENT_REDRAW:
		if(device->redraw) {
			device->redraw = false;
			object_wait_callback(wait);
		} else {
			notifier_register(&device->redraw_notifier, object_wait_notifier, wait);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_PARAM_INVAL;
	}
}

/** Stop waiting for a display device event.
 * @param _device	Device to stop waiting for.
 * @param data		Unused.
 * @param wait		Wait information structure. */
static void display_device_unwait(device_t *_device, void *data, object_wait_t *wait) {
	display_device_t *device = _device->data;

	switch(wait->event) {
	case DISPLAY_EVENT_REDRAW:
		notifier_unregister(&device->redraw_notifier, object_wait_notifier, wait);
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
	return device->ops->get_page(device, offset, physp);
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
static status_t display_device_request(device_t *_device, void *data, int request, void *in,
                                       size_t insz, void **outp, size_t *outszp) {
	display_device_t *device = _device->data;
	display_mode_t *mode = NULL;
	phys_ptr_t phys;
	status_t ret;
	uint16_t id;

	switch(request) {
	case DISPLAY_MODE_COUNT:
		if(!outp || !outszp) {
			return STATUS_PARAM_INVAL;
		}
		*outp = kmemdup(&device->count, sizeof(size_t), MM_SLEEP);
		*outszp = sizeof(size_t);
		return STATUS_SUCCESS;
	case DISPLAY_GET_MODES:
		if(!outp || !outszp) {
			return STATUS_PARAM_INVAL;
		}
		*outp = kmemdup(device->modes, sizeof(display_mode_t) * device->count, MM_SLEEP);
		*outszp = sizeof(display_mode_t) * device->count;
		return STATUS_SUCCESS;
	case DISPLAY_GET_PREFERRED_MODE:
		if(!outp || !outszp) {
			return STATUS_PARAM_INVAL;
		}

		mutex_lock(&device->lock);

		/* For now just return whatever mode the kernel console is
		 * using, and fallback on 1024x768, then 800x600 if the mode
		 * is unavailable. */
		mode = display_mode_find(device, fb_console_width, fb_console_height, fb_console_depth);
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

		*outp = kmemdup(mode, sizeof(display_mode_t), MM_SLEEP);
		*outszp = sizeof(display_mode_t);
		mutex_unlock(&device->lock);
		return STATUS_SUCCESS;
	case DISPLAY_SET_MODE:
		if(in && insz != sizeof(uint16_t)) {
			return STATUS_PARAM_INVAL;
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
				ret = device->ops->get_page(device, mode->offset, &phys);
				if(ret != STATUS_SUCCESS) {
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
		return STATUS_SUCCESS;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->ops->request) {
			mutex_lock(&device->lock);
			ret = device->ops->request(device, request, in, insz, outp, outszp);
			mutex_unlock(&device->lock);
			return ret;
		} else {
			return STATUS_PARAM_INVAL;
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
                               device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "display" } },
	};
	char dname[DEVICE_NAME_MAX];
	display_device_t *device;
	status_t ret;

	if((parent && !name) || (name && !parent) || !ops || !modes || !count || !devicep) {
		return STATUS_PARAM_INVAL;
	}

	device = kmalloc(sizeof(display_device_t), MM_SLEEP);
	mutex_init(&device->lock, "display_device_lock", 0);
	atomic_set(&device->open, 0);
	notifier_init(&device->redraw_notifier, device);
	device->id = atomic_inc(&next_display_id);
	device->ops = ops;
	device->data = data;
	device->modes = kmemdup(modes, sizeof(display_mode_t) * count, MM_SLEEP);
	device->count = count;
	device->curr_mode = NULL;
	device->redraw = false;

	/* Create the device tree node. */
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		ret = device_create(name, parent, &display_device_ops, device, attrs,
		                    ARRAYSZ(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}

		/* Should not fail - only possible failure is if name already
		 * exists, and ID should be unique. */
		device_alias(dname, display_device_dir, *devicep, NULL);
	} else {
		ret = device_create(dname, display_device_dir, &display_device_ops, device,
		                    attrs, ARRAYSZ(attrs), devicep);
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
