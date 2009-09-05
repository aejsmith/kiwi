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

#include <drivers/display.h>

#include <lib/utility.h>
#include <lib/string.h>

#include <mm/malloc.h>

#include <types/atomic.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>

/** Display device directory. */
static device_t *display_device_dir;

/** Next device ID. */
static atomic_t display_next_id = 0;

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
	identifier_t id;
	size_t i;

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
		if(!in || insz != sizeof(identifier_t)) {
			return -ERR_PARAM_INVAL;
		} else if(!device->ops->mode_set) {
			return -ERR_NOT_SUPPORTED;
		}

		id = *(identifier_t *)in;
		for(i = 0; i < device->count; i++) {
			if(device->modes[i].id == id) {
				return device->ops->mode_set(device, &device->modes[i]);
			}
		}

		return -ERR_NOT_FOUND;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->ops->request) {
			return device->ops->request(device, request, in, insz, outp, outszp);
		}
		return -ERR_PARAM_INVAL;
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
	atomic_set(&device->open, 0);
	device->id = atomic_inc(&display_next_id);
	device->ops = ops;
	device->data = data;
	device->modes = kmemdup(modes, sizeof(display_mode_t) * count, MM_SLEEP);
	device->count = count;

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
