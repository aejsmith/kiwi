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
 * @brief		Input device manager.
 */

#include <drivers/input.h>

#include <io/device.h>

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

/** Input device directory. */
static device_t *input_device_dir;

/** Next device ID. */
static atomic_t next_input_id = 0;

/** Destroy an input device.
 * @param _device	Device to destroy. */
static void input_device_destroy(device_t *_device) {
	input_device_t *device = _device->data;

	assert(!device->open);

	switch(device->type) {
	case INPUT_TYPE_KEYBOARD:
		if(device->kops->destroy) {
			device->kops->destroy(device);
		}
		break;
	case INPUT_TYPE_MOUSE:
		if(device->mops->destroy) {
			device->mops->destroy(device);
		}
		break;
	}

	kfree(device);
}

/** Open an input device.
 * @param _device	Device being opened.
 * @param datap		Where to store handle-specific data pointer (unused).
 * @return		Status code describing result of the operation. */
static status_t input_device_open(device_t *_device, void **datap) {
	input_device_t *device = _device->data;

	if(!atomic_cmp_set(&device->open, 0, 1)) {
		return STATUS_IN_USE;
	}

	return STATUS_SUCCESS;
}

/** Close an input device.
 * @param _device	Device being closed.
 * @param data		Handle-specific data pointer (unused).
 * @return		Status code describing result of the operation. */
static void input_device_close(device_t *_device, void *data) {
	input_device_t *device = _device->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);
}

/** Read from an input device.
 * @param _device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param _buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (ignored).
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t input_device_read(device_t *_device, void *data, void *_buf, size_t count,
                                  offset_t offset, size_t *bytesp) {
	input_device_t *device = _device->data;
	status_t ret = STATUS_SUCCESS;
	char *buf = _buf;
	size_t i;

	for(i = 0; i < count; i++) {
		ret = semaphore_down_etc(&device->sem, -1, SYNC_INTERRUPTIBLE);
		if(ret != STATUS_SUCCESS) {
			break;
		}

		spinlock_lock(&device->lock);

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
 * @param _device	Device to wait for.
 * @param data		Handle-specific data pointer (unused).
 * @param wait		Wait information structure.
 * @return		Status code describing result of the operation. */
static status_t input_device_wait(device_t *_device, void *data, object_wait_t *wait) {
	input_device_t *device = _device->data;

	switch(wait->event) {
	case DEVICE_EVENT_READABLE:
		if(semaphore_count(&device->sem)) {
			object_wait_callback(wait);
		} else {
			notifier_register(&device->data_notifier, object_wait_notifier, wait);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_PARAM_INVAL;
	}
}

/** Stop waiting for an input device event.
 * @param _device	Device to stop waiting for.
 * @param data		Handle-specific data pointer (unused).
 * @param wait		Wait information structure. */
static void input_device_unwait(device_t *_device, void *data, object_wait_t *wait) {
	input_device_t *device = _device->data;

	switch(wait->event) {
	case DEVICE_EVENT_READABLE:
		notifier_unregister(&device->data_notifier, object_wait_notifier, wait);
		break;
	}
}

/** Handler for keyboard requests.
 * @param _device	Device request is being made on.
 * @param data		Handle-specific data pointer (unused).
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t keyboard_device_request(device_t *_device, void *data, int request, void *in,
                                        size_t insz, void **outp, size_t *outszp) {
	input_device_t *device = _device->data;

	switch(request) {
	case KEYBOARD_SET_LEDS:
		if(insz != sizeof(keyboard_led_state_t)) {
			return STATUS_PARAM_INVAL;
		} else if(!device->kops || !device->kops->set_leds) {
			return STATUS_NOT_SUPPORTED;
		}

		device->kops->set_leds(device, in);
		return STATUS_SUCCESS;
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->kops->request) {
			return device->kops->request(device, request, in, insz, outp, outszp);
		}
		return STATUS_PARAM_INVAL;
	}
}

/** Handler for mouse requests.
 * @param _device	Device request is being made on.
 * @param data		Handle-specific data pointer (unused).
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t mouse_device_request(device_t *_device, void *data, int request, void *in,
                                     size_t insz, void **outp, size_t *outszp) {
	input_device_t *device = _device->data;

	switch(request) {
	default:
		if(request >= DEVICE_CUSTOM_REQUEST_START && device->mops->request) {
			return device->mops->request(device, request, in, insz, outp, outszp);
		}
		return STATUS_PARAM_INVAL;
	}
}

/** Keyboard device operations. */
static device_ops_t keyboard_device_ops = {
	.destroy = input_device_destroy,
	.open = input_device_open,
	.close = input_device_close,
	.read = input_device_read,
	.wait = input_device_wait,
	.unwait = input_device_unwait,
	.request = keyboard_device_request,
};

/** Mouse device operations. */
static device_ops_t mouse_device_ops = {
	.destroy = input_device_destroy,
	.open = input_device_open,
	.close = input_device_close,
	.read = input_device_read,
	.wait = input_device_wait,
	.unwait = input_device_unwait,
	.request = mouse_device_request,
};

/** Add data to an input device's buffer.
 *
 * Inserts data into an input device's data buffer. This function is safe to
 * use from interrupt context.
 *
 * @param device	Device to add to.
 * @param value		Value to add.
 */
void input_device_input(device_t *_device, uint8_t value) {
	input_device_t *device = _device->data;

	spinlock_lock(&device->lock);

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
 * @return		Status code describing result of the operation. */
static status_t input_device_create(const char *name, device_t *parent, uint8_t type,
                                    uint8_t protocol, void *ops, void *data,
                                    device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "input" } },
		{ "input.type", DEVICE_ATTR_UINT8, { .uint8 = type } },
		{ "input.protocol", DEVICE_ATTR_UINT8, { .uint8 = protocol } },
	};
	char dname[DEVICE_NAME_MAX];
	input_device_t *device;
	device_ops_t *iops;
	status_t ret;

	if((parent && !name) || (name && !parent) || !devicep) {
		return STATUS_PARAM_INVAL;
	}

	device = kmalloc(sizeof(input_device_t), MM_SLEEP);
	spinlock_init(&device->lock, "input_device_lock");
	semaphore_init(&device->sem, "input_device_sem", 0);
	notifier_init(&device->data_notifier, device);
	device->id = atomic_inc(&next_input_id);
	device->ops = ops;
	device->data = data;
	device->open = 0;
	device->type = type;
	device->start = 0;
	device->size = 0;

	/* Create the device tree node. */
	iops = (type == INPUT_TYPE_KEYBOARD) ? &keyboard_device_ops : &mouse_device_ops;
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		ret = device_create(name, parent, iops, device, attrs, ARRAYSZ(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}

		ret = device_alias(dname, input_device_dir, *devicep, NULL);
		if(ret != STATUS_SUCCESS) {
			/* Should not fail - only possible failure is if name
			 * already exists, and ID should be unique. Note that
			 * with current ID allocation implementation this can
			 * happen - FIXME. */
			fatal("Could not create device alias (%d)", ret);
		}
	} else {
		ret = device_create(dname, input_device_dir, iops, device, attrs,
	                            ARRAYSZ(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}
	}

	return STATUS_SUCCESS;
}

/** Add a new keyboard device.
 *
 * Adds a new keyboard device to the device tree. If specified, the device will
 * be created as a child of a specific device, and an alias will be created
 * under the input device tree. Otherwise, the main device will be placed in
 * the input device tree.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the input device
 *			container.
 * @param protocol	Device protocol.
 * @param ops		Pointer to operations structure.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t keyboard_device_create(const char *name, device_t *parent, uint8_t protocol,
                                keyboard_ops_t *ops, void *data, device_t **devicep) {
	return input_device_create(name, parent, INPUT_TYPE_KEYBOARD, protocol, ops, data, devicep);
}
MODULE_EXPORT(keyboard_device_create);

/** Add a new mouse device.
 *
 * Adds a new mouse device to the device tree. If specified, the device will
 * be created as a child of a specific device, and an alias will be created
 * under the input device tree. Otherwise, the main device will be placed in
 * the input device tree.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the input device
 *			container.
 * @param protocol	Device protocol.
 * @param ops		Pointer to operations structure.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t mouse_device_create(const char *name, device_t *parent, uint8_t protocol,
                             mouse_ops_t *ops, void *data, device_t **devicep) {
	return input_device_create(name, parent, INPUT_TYPE_MOUSE, protocol, ops, data, devicep);
}
MODULE_EXPORT(mouse_device_create);

/** Initialisation function for the input module.
 * @return		Status code describing result of the operation. */
static status_t input_init(void) {
	/* Create the input device directory. */
	return device_create("input", device_tree_root, NULL, NULL, NULL, 0, &input_device_dir);
}

/** Unloading function for the input module.
 * @return		Status code describing result of the operation. */
static status_t input_unload(void) {
	return STATUS_SUCCESS;
}

MODULE_NAME("input");
MODULE_DESC("Input device class manager");
MODULE_FUNCS(input_init, input_unload);
