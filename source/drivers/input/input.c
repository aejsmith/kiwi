/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Input device manager.
 */

#include <drivers/input.h>

#include <io/device.h>
#include <io/request.h>

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>
#include <time.h>

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
		if(device->kops->destroy)
			device->kops->destroy(device);

		break;
	case INPUT_TYPE_MOUSE:
		if(device->mops->destroy)
			device->mops->destroy(device);

		break;
	}

	kfree(device);
}

/** Open an input device.
 * @param _device	Device being opened.
 * @param flags		Flags being opened with.
 * @param datap		Where to store handle-specific data pointer.
 * @return		Status code describing result of the operation. */
static status_t input_device_open(device_t *_device, uint32_t flags, void **datap) {
	input_device_t *device = _device->data;

	if(atomic_cas(&device->open, 0, 1) != 0)
		return STATUS_IN_USE;

	return STATUS_SUCCESS;
}

/** Close an input device.
 * @param _device	Device being closed.
 * @param handle	File handle structure. */
static void input_device_close(device_t *_device, file_handle_t *handle) {
	input_device_t *device = _device->data;
	int old;

	old = atomic_dec(&device->open);
	assert(old == 1);
}

/** Signal that an input device event is being waited for.
 * @param _device	Device to wait for.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer
 * @return		Status code describing result of the operation. */
static status_t input_device_wait(device_t *_device, file_handle_t *handle,
	unsigned event, void *wait)
{
	input_device_t *device = _device->data;

	switch(event) {
	case FILE_EVENT_READABLE:
		if(semaphore_count(&device->sem)) {
			object_wait_signal(wait, 0);
		} else {
			notifier_register(&device->data_notifier, object_wait_notifier, wait);
		}

		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for an input device event.
 * @param _device	Device to stop waiting for.
 * @param handle	File handle structure.
 * @param event		Event that is being waited for.
 * @param wait		Internal data pointer. */
static void input_device_unwait(device_t *_device, file_handle_t *handle,
	unsigned event, void *wait)
{
	input_device_t *device = _device->data;

	switch(event) {
	case FILE_EVENT_READABLE:
		notifier_unregister(&device->data_notifier, object_wait_notifier, wait);
		break;
	}
}

/** Perform I/O on an input device.
 * @param _device	Device to perform I/O on.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t input_device_io(device_t *_device, file_handle_t *handle, io_request_t *request) {
	input_device_t *device = _device->data;
	size_t i;
	input_event_t event;
	status_t ret;

	if(request->op == IO_OP_WRITE)
		return STATUS_NOT_SUPPORTED;

	if(request->total % sizeof(input_event_t))
		return STATUS_INVALID_ARG;

	for(i = 0; i < request->total / sizeof(input_event_t); i++) {
		ret = semaphore_down_etc(&device->sem, -1, SLEEP_INTERRUPTIBLE);
		if(ret != STATUS_SUCCESS)
			return ret;

		spinlock_lock(&device->lock);

		memcpy(&event, &device->buffer[device->start], sizeof(event));
		device->size--;
		if(++device->start == INPUT_BUFFER_SIZE)
			device->start = 0;

		spinlock_unlock(&device->lock);

		ret = io_request_copy(request, &event, sizeof(event));
		if(ret != STATUS_SUCCESS)
			return ret;
	}

	return STATUS_SUCCESS;
}

/** Handler for keyboard requests.
 * @param _device	Device request is being made on.
 * @param handle	File handle structure.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param in_size	Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param out_sizep	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t keyboard_device_request(device_t *_device, file_handle_t *handle,
	unsigned request, const void *in, size_t in_size, void **outp,
	size_t *out_sizep)
{
	input_device_t *device = _device->data;

	if(request >= DEVICE_CUSTOM_REQUEST_START && device->kops->request)
		return device->kops->request(device, request, in, in_size, outp, out_sizep);

	return STATUS_INVALID_REQUEST;
}

/** Handler for mouse requests.
 * @param _device	Device request is being made on.
 * @param handle	File handle structure.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param in_size	Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param out_sizep	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t mouse_device_request(device_t *_device, file_handle_t *handle,
	unsigned request, const void *in, size_t in_size, void **outp,
	size_t *out_sizep)
{
	input_device_t *device = _device->data;

	if(request >= DEVICE_CUSTOM_REQUEST_START && device->mops->request)
		return device->mops->request(device, request, in, in_size, outp, out_sizep);

	return STATUS_INVALID_REQUEST;
}

/** Keyboard device operations. */
static device_ops_t keyboard_device_ops = {
	.type = FILE_TYPE_CHAR,
	.destroy = input_device_destroy,
	.open = input_device_open,
	.close = input_device_close,
	.wait = input_device_wait,
	.unwait = input_device_unwait,
	.io = input_device_io,
	.request = keyboard_device_request,
};

/** Mouse device operations. */
static device_ops_t mouse_device_ops = {
	.type = FILE_TYPE_CHAR,
	.destroy = input_device_destroy,
	.open = input_device_open,
	.close = input_device_close,
	.wait = input_device_wait,
	.unwait = input_device_unwait,
	.io = input_device_io,
	.request = mouse_device_request,
};

/**
 * Add an event to an input device's buffer.
 *
 * Adds an event to an input device's event buffer. This function is safe to
 * use from interrupt context.
 *
 * @param _device	Device to add to.
 * @param type		Type of event.
 * @param value		Value of event.
 */
__export void input_device_event(device_t *_device, uint8_t type, int32_t value) {
	input_device_t *device = _device->data;
	size_t i;

	spinlock_lock(&device->lock);

	/* Drop the input if full or device is not open. */
	if(!atomic_get(&device->open) || device->size == INPUT_BUFFER_SIZE) {
		spinlock_unlock(&device->lock);
		return;
	}

	i = (device->start + device->size++) % INPUT_BUFFER_SIZE;
	device->buffer[i].time = system_time();
	device->buffer[i].type = type;
	device->buffer[i].value = value;

	semaphore_up(&device->sem, 1);
	notifier_run_unlocked(&device->data_notifier, NULL, false);
	spinlock_unlock(&device->lock);
}

/** Add a new input device.
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the input device container.
 * @param type		Input device type.
 * @param ops		Pointer to operations structure for device type.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 * @return		Status code describing result of the operation. */
static status_t input_device_create(const char *name, device_t *parent, uint8_t type,
	void *ops, void *data, device_t **devicep)
{
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "input" } },
		{ "input.type", DEVICE_ATTR_UINT8, { .uint8 = type } },
	};
	char dname[DEVICE_NAME_MAX];
	input_device_t *device;
	device_ops_t *iops;
	status_t ret;

	if((parent && !name) || (name && !parent) || !devicep)
		return STATUS_INVALID_ARG;

	device = kmalloc(sizeof(input_device_t), MM_WAIT);
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
		ret = device_create(name, parent, iops, device, attrs,
			ARRAY_SIZE(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}

		/* Should not fail - only possible failure is if name already
		 * exists, and ID should be unique. */
		device_alias(dname, input_device_dir, *devicep, NULL);
	} else {
		ret = device_create(dname, input_device_dir, iops, device, attrs,
			ARRAY_SIZE(attrs), devicep);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}
	}

	return STATUS_SUCCESS;
}

/**
 * Add a new keyboard device.
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
 * @param ops		Pointer to operations structure.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		Status code describing result of the operation.
 */
__export status_t keyboard_device_create(const char *name, device_t *parent,
	keyboard_ops_t *ops, void *data, device_t **devicep)
{
	return input_device_create(name, parent, INPUT_TYPE_KEYBOARD, ops, data, devicep);
}

/**
 * Add a new mouse device.
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
 * @param ops		Pointer to operations structure.
 * @param data		Data specific to the device.
 * @param parent	Optional parent device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		Status code describing result of the operation.
 */
__export status_t mouse_device_create(const char *name, device_t *parent,
	mouse_ops_t *ops, void *data, device_t **devicep)
{
	return input_device_create(name, parent, INPUT_TYPE_MOUSE, ops, data, devicep);
}

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
