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
 * @brief		Console device manager.
 */

#include <drivers/console.h>

#include <io/device.h>

#include <lib/string.h>

#include <ipc/pipe.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

/** Console device data structure. */
typedef struct console_device {
	int id;				/**< ID of console. */
	pipe_t *input;			/**< Buffer contaning input for slave. */
	pipe_t *output;			/**< Buffer containing output from slave. */
	device_t *slave;		/**< Slave device. */
} console_device_t;

/** Console device directory/console master device. */
static device_t *console_device_dir;
static device_t *console_master_device;

/** Next console ID. */
static atomic_t next_console_id = 0;

/** Read from a console slave device.
 * @param device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t console_slave_read(device_t *device, void *data, void *buf, size_t count,
                                   offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;
	return pipe_read(console->input, buf, count, false, bytesp);
}

/** Write to a console slave device.
 * @param device	Device to write to.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t console_slave_write(device_t *device, void *data, const void *buf, size_t count,
                                    offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;
	return pipe_write(console->output, buf, count, false, bytesp);
}

/** Signal that a console slave event is being waited for.
 * @param device	Device to wait for.
 * @param data		Handle-specific data pointer (unused).
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer.
 * @return		Status code describing result of the operation. */
static status_t console_slave_wait(device_t *device, void *data, int event, void *sync) {
	console_device_t *console = device->data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_wait(console->input, false, sync);
		return STATUS_SUCCESS;
	case DEVICE_EVENT_WRITABLE:
		pipe_wait(console->output, true, sync);
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a console slave event.
 * @param device	Device to stop waiting for.
 * @param data		Handle-specific data pointer (unused).
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer. */
static void console_slave_unwait(device_t *device, void *data, int event, void *sync) {
	console_device_t *console = device->data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_unwait(console->input, false, sync);
		break;
	case DEVICE_EVENT_WRITABLE:
		pipe_unwait(console->output, true, sync);
		break;
	}
}

/** Slave console device operations. */
static device_ops_t console_slave_ops = {
	.read = console_slave_read,
	.write = console_slave_write,
	.wait = console_slave_wait,
	.unwait = console_slave_unwait,
};

/** Open the console master device.
 * @param device	Device being obtained.
 * @param datap		Where to store handle-specific data pointer.
 * @return		Status code describing result of the operation. */
static status_t console_master_open(device_t *device, void **datap) {
	char name[DEVICE_NAME_MAX];
	console_device_t *console;
	status_t ret;

	/* Create a new console .*/
	console = kmalloc(sizeof(console_device_t), MM_SLEEP);
	console->id = atomic_inc(&next_console_id);
	console->input = pipe_create();
	console->output = pipe_create();
	
	sprintf(name, "%d", console->id); 
	ret = device_create(name, console_device_dir, &console_slave_ops, console, NULL,
	                    0, &console->slave);
	if(ret != STATUS_SUCCESS) {
		kfree(console);
		return ret;
	}

	*datap = console;
	return STATUS_SUCCESS;
}

/** Close the console master device.
 * @param device	Device being closed.
 * @param data		Pointer to console structure. */
static void console_master_close(device_t *device, void *data) {
	console_device_t *console = data;

	/* FIXME: Device manager doesn't allow removal of in-use devices yet. */
	if(device_destroy(console->slave) == STATUS_SUCCESS) {
		pipe_destroy(console->input);
		pipe_destroy(console->output);
		kfree(console);
	}
}

/** Read from the console master device.
 * @param device	Device to read from.
 * @param data		Pointer to console structure.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t console_master_read(device_t *device, void *data, void *buf, size_t count,
                                    offset_t offset, size_t *bytesp) {
	console_device_t *console = data;
	return pipe_read(console->output, buf, count, false, bytesp);
}

/** Write to the console master device.
 * @param device	Device to write to.
 * @param data		Pointer to console structure.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t console_master_write(device_t *device, void *data, const void *buf, size_t count,
                                     offset_t offset, size_t *bytesp) {
	console_device_t *console = data;
	return pipe_write(console->input, buf, count, false, bytesp);
}

/** Signal that a console master event is being waited for.
 * @param device	Device to wait for.
 * @param data		Pointer to console structure.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer.
 * @return		Status code describing result of the operation. */
static status_t console_master_wait(device_t *device, void *data, int event, void *sync) {
	console_device_t *console = data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_wait(console->output, false, sync);
		return STATUS_SUCCESS;
	case DEVICE_EVENT_WRITABLE:
		pipe_wait(console->input, true, sync);
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a console master event.
 * @param device	Device to stop waiting for.
 * @param data		Pointer to console structure.
 * @param event		Event to wait for.
 * @param sync		Synchronisation pointer. */
static void console_master_unwait(device_t *device, void *data, int event, void *sync) {
	console_device_t *console = data;

	switch(event) {
	case DEVICE_EVENT_READABLE:
		pipe_unwait(console->output, false, sync);
		break;
	case DEVICE_EVENT_WRITABLE:
		pipe_unwait(console->input, true, sync);
		break;
	}
}

/** Handler for console master requests.
 * @param device	Device request is being made on.
 * @param data		Pointer to console structure.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t console_master_request(device_t *device, void *data, int request, void *in,
                                       size_t insz, void **outp, size_t *outszp) {
	console_device_t *console = data;

	switch(request) {
	case CONSOLE_MASTER_GET_ID:
		*outp = kmemdup(&console->id, sizeof(console->id), MM_SLEEP);
		*outszp = sizeof(console->id);
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_REQUEST;
	}
}

/** Console master device operations. */
static device_ops_t console_master_ops = {
	.open = console_master_open,
	.close = console_master_close,
	.read = console_master_read,
	.write = console_master_write,
	.wait = console_master_wait,
	.unwait = console_master_unwait,
	.request = console_master_request,
};

/** Initialisation function for the console driver.
 * @return		Status code describing result of the operation. */
static status_t console_init(void) {
	status_t ret;

	/* Create console directory. */
	ret = device_create("console", device_tree_root, NULL, NULL, NULL, 0, &console_device_dir);	
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Create console manager device. */
	ret = device_create("master", console_device_dir, &console_master_ops,
	                    NULL, NULL, 0, &console_master_device);
	if(ret != STATUS_SUCCESS) {
		device_destroy(console_device_dir);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Unloading function for the console driver.
 * @return		Status code describing result of the operation. */
static status_t console_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("console");
MODULE_DESC("Console device manager");
MODULE_FUNCS(console_init, console_unload);
