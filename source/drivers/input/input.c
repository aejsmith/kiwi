/* Kiwi input device manager
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
 * @brief		Input device manager.
 *
 * The kernel side of input handling is very simple - most of the work is
 * offloaded to userspace. All we do is publish a device with attributes to
 * specify what type of device it is and what protocol it is using, and provide
 * raw data from the device to read calls on the device. We also provide
 * requests to do things such as set keyboard LED state, etc.
 */

#include <drivers/input.h>

#include <io/device.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <types/atomic.h>

#include <assert.h>
#include <errors.h>
#include <module.h>

/** Input device type structure. */
typedef struct input_type {
	/** Handler for type-specific requests.
	 * @param device	Device request is being made on.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param insz		Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param outszp	Where to store output buffer size.
	 * @return		Positive value on success, negative error code
	 *			on failure. */
	int (*request)(input_device_t *device, int request, void *in, size_t insz, void **outp, size_t *outszp);
} input_type_t;

/** Input device directory. */
static device_t *input_device_dir;

/** Next device ID. */
static atomic_t input_next_id = 0;

#if 0
# pragma mark Keyboard device type.
#endif

/** Handler for keyboard requests.
 * @param device	Device request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		0 value on success, negative error code on failure. */
static int input_keyboard_request(input_device_t *device, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	input_kb_ops_t *ops = device->ops;

	switch(request) {
	case INPUT_KB_SET_LEDS:
		if(insz != sizeof(input_kb_led_state_t)) {
			return -ERR_PARAM_INVAL;
		} else if(!ops || !ops->set_leds) {
			return -ERR_NOT_SUPPORTED;
		}

		ops->set_leds(device, in);
		return 0;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && ops->request) {
			return ops->request(device, request, in, insz, outp, outszp);
		}
		return -ERR_PARAM_INVAL;
	}
}

/** Keyboard device type structure. */
static input_type_t input_keyboard_type = {
	.request = input_keyboard_request,
};

#if 0
# pragma mark Mouse device type.
#endif

/** Handler for mouse requests.
 * @param device	Device request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		0 value on success, negative error code on failure. */
static int input_mouse_request(input_device_t *device, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	input_mouse_ops_t *ops = device->ops;

	switch(request) {
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && ops->request) {
			return ops->request(device, request, in, insz, outp, outszp);
		}
		return -ERR_PARAM_INVAL;
	}
}

/** Keyboard device type structure. */
static input_type_t input_mouse_type = {
	.request = input_mouse_request,
};

#if 0
# pragma mark -
#endif

/** Open an input device.
 * @param _dev		Device being opened.
 * @return		0 on success, negative error code on failure. */
static int input_device_get(device_t *_dev) {
	input_device_t *device = _dev->data;

	if(!atomic_cmp_set(&device->open, 0, 1)) {
		return -ERR_IN_USE;
	}

	return 0;
}

/** Close an input device.
 * @param _dev		Device being closed.
 * @return		0 on success, negative error code on failure. */
static void input_device_release(device_t *_dev) {
	input_device_t *device = _dev->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);
}

/** Read from an input device.
 * @param _dev		Device to read from.
 * @param _buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (ignored).
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
static int input_device_read(device_t *_dev, void *_buf, size_t count, offset_t offset, size_t *bytesp) {
	input_device_t *device = _dev->data;
	char *buf = _buf;
	int ret = 0;
	size_t i;

	for(i = 0; i < count; i++) {
		if((ret = semaphore_down(&device->sem, SYNC_INTERRUPTIBLE)) != 0) {
			break;
		}

		spinlock_lock(&device->lock, 0);

		buf[i] = device->buffer[device->start];

		device->size--;
		if(++device->start == INPUT_BUFFER_SIZE) {
			device->start = 0;
		}

		spinlock_unlock(&device->lock);
	}

	*bytesp = i;
	return ret;
}

/** Signal that an input device event is being waited for.
 * @param _dev		Device to wait for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int input_device_wait(device_t *_dev, handle_wait_t *wait) {
	input_device_t *device = _dev->data;

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		if(device->sem.queue.missed) {
			wait->cb(wait);
		} else {
			notifier_register(&device->data_notifier, handle_wait_notifier, wait);
		}
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Stop waiting for an input device event.
 * @param _dev		Device to stop waiting for.
 * @param wait		Wait information structure. */
static void input_device_unwait(device_t *_dev, handle_wait_t *wait) {
	input_device_t *device = _dev->data;

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		notifier_unregister(&device->data_notifier, handle_wait_notifier, wait);
		break;
	}
}

/** Handler for input device requests.
 * @param _dev		Device request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Positive value on success, negative error code on
 *			failure. */
static int input_device_request(device_t *_dev, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	input_device_t *device = _dev->data;

	return device->type->request(device, request, in, insz, outp, outszp);
}

/** Input device operations. */
static device_ops_t input_device_ops = {
	.get = input_device_get,
	.release = input_device_release,
	.read = input_device_read,
	.wait = input_device_wait,
	.unwait = input_device_unwait,
	.request = input_device_request,
};

/** Add data to an input device's buffer.
 *
 * Inserts data into an input device's data buffer. This function is safe to
 * use from interrupt context.
 *
 * @param device	Device to add to.
 * @param value		Value to add.
 */
void input_device_input(input_device_t *device, uint8_t value) {
	spinlock_lock(&device->lock, 0);

	/* Drop the input if full or device is not open. */
	if(!atomic_get(&device->open) || device->size == INPUT_BUFFER_SIZE) {
		spinlock_unlock(&device->lock);
		return;
	}

	device->buffer[(device->start + device->size++) % INPUT_BUFFER_SIZE] = value;
	semaphore_up(&device->sem, 1);
	notifier_run_unlocked(&device->data_notifier, NULL, false);
	spinlock_unlock(&device->lock);
}
MODULE_EXPORT(input_device_input);

/** Add a new input device.
 *
 * Adds a new input device to the device tree. If specified, the device will
 * be created as a child of a specific device, and an alias will be created
 * under the input device tree. Otherwise, the main device will be placed in
 * the input device tree.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the input device
 *			container.
 * @param type		Input device type.
 * @param protocol	Device protocol.
 * @param ops		Pointer to operations structure for device type.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int input_device_create(const char *name, device_t *parent, uint8_t type,
                        uint8_t protocol, void *ops, void *data,
                        input_device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "input" } },
		{ "input.type", DEVICE_ATTR_UINT8, { .uint8 = type } },
		{ "input.protocol", DEVICE_ATTR_UINT8, { .uint8 = protocol } },
	};
	char dname[DEVICE_NAME_MAX];
	input_device_t *device;
	int ret;

	if((parent && !name) || (name && !parent) || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	device = kmalloc(sizeof(input_device_t), MM_SLEEP);
	spinlock_init(&device->lock, "input_device_lock");
	semaphore_init(&device->sem, "input_device_sem", 0);
	notifier_init(&device->data_notifier, device);
	device->id = atomic_inc(&input_next_id);
	device->ops = ops;
	device->data = data;
	device->open = 0;
	device->start = 0;
	device->size = 0;

	/* Find the device type. */
	switch(type) {
	case INPUT_TYPE_KEYBOARD:
		device->type = &input_keyboard_type;
		break;
	case INPUT_TYPE_MOUSE:
		device->type = &input_mouse_type;
		break;
	default:
		kfree(device);
		return -ERR_PARAM_INVAL;
	}

	/* Create the device tree node. */
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		if((ret = device_create(name, parent, &input_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		} else if((ret = device_alias(dname, input_device_dir, device->device, &device->alias)) != 0) {
			/* Should not fail - only possible failure is if name
			 * already exists, and ID should be unique. Note that
			 * with current ID allocation implementation this can
			 * happen - FIXME. */
			fatal("Could not create device alias (%d)", ret);
		}
	} else {
		if((ret = device_create(dname, input_device_dir, &input_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		}
		device->alias = NULL;
	}

	*devicep = device;
	return 0;
}
MODULE_EXPORT(input_device_create);

/** Remove an input device.
 *
 * Removes an input device from the device tree and frees it.
 *
 * @param device	Device to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int input_device_destroy(input_device_t *device) {
	int ret;

	assert(!device->open);

	if(device->alias) {
		device_destroy(device->alias);
	}

	if((ret = device_destroy(device->device)) != 0) {
		return ret;
	}

	kfree(device);
	return 0;
}
MODULE_EXPORT(input_device_destroy);

/** Initialisation function for the input module.
 * @return		0 on success, negative error code on failure. */
static int input_init(void) {
	/* Create the input device directory. */
	return device_create("input", device_tree_root, NULL, NULL, NULL, 0, &input_device_dir);
}

/** Unloading function for the input module.
 * @return		0 on success, negative error code on failure. */
static int input_unload(void) {
	return 0;
}

MODULE_NAME("input");
MODULE_DESC("Input device class manager");
MODULE_FUNCS(input_init, input_unload);
