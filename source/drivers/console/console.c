/* Kiwi console device manager
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
 * @brief		Console device manager.
 *
 * @todo		Destroying devices.
 */

#include <console/kprintf.h>

#include <drivers/console.h>

#include <io/device.h>
#include <io/pipe.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <errors.h>
#include <module.h>

/** Console device data structure. */
typedef struct console_device {
	int id;				/**< ID of console. */
	bool mopen;			/**< Whether the master device is open. */

	pipe_t *input;			/**< Buffer contaning input for slave. */
	pipe_t *output;			/**< Buffer containing output from slave. */

	device_t *dir;			/**< Device directory. */
	device_t *master;		/**< Master device. */
	device_t *slave;		/**< Slave device. */
} console_device_t;

/** Console device directory/console manager device. */
static device_t *console_device_dir;
static device_t *console_manager_device;

/** Next console ID. */
static atomic_t console_next_id = 0;

/** Get a console master device.
 * @param device	Device being obtained.
 * @return		0 on success, negative error code on failure. */
static int console_master_get(device_t *device) {
	console_device_t *console = device->data;

	if(console->mopen) {
		return -ERR_IN_USE;
	}

	console->mopen = true;
	return 0;
}

/** Release a console master device.
 * @param device	Device being released. */
static void console_master_release(device_t *device) {
	console_device_t *console = device->data;

	console->mopen = false;
}

/** Read from a console master device.
 * @param device	Device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
static int console_master_read(device_t *device, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;

	return pipe_read(console->output, buf, count, false, bytesp);
}

/** Write to a console master device.
 * @param device	Device to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
static int console_master_write(device_t *device, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;

	return pipe_write(console->input, buf, count, false, bytesp);
}

/** Signal that a console master event is being waited for.
 * @param device	Device to wait for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int console_master_wait(device_t *device, handle_wait_t *wait) {
	console_device_t *console = device->data;

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		pipe_wait(console->output, false, wait);
		return 0;
	case HANDLE_EVENT_WRITE:
		pipe_wait(console->input, true, wait);
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Stop waiting for a console master event.
 * @param device	Device to stop waiting for.
 * @param wait		Wait information structure. */
static void console_master_unwait(device_t *device, handle_wait_t *wait) {
	console_device_t *console = device->data;

	switch(wait->event) {
	case HANDLE_EVENT_READ:
		pipe_unwait(console->output, false, wait);
		break;
	case HANDLE_EVENT_WRITE:
		pipe_unwait(console->input, true, wait);
		break;
	}
}

/** Master console device operations. */
static device_ops_t console_master_ops = {
	.get = console_master_get,
	.release = console_master_release,
	.read = console_master_read,
	.write = console_master_write,
	.wait = console_master_wait,
	.unwait = console_master_unwait,
};

/** Read from a console slave device.
 * @param device	Device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
static int console_slave_read(device_t *device, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;

	return pipe_read(console->input, buf, count, false, bytesp);
}

/** Write to a console slave device.
 * @param device	Device to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to (unused).
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
static int console_slave_write(device_t *device, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	console_device_t *console = device->data;

	return pipe_write(console->output, buf, count, false, bytesp);
}

/** Slave console device operations. */
static device_ops_t console_slave_ops = {
	.read = console_slave_read,
	.write = console_slave_write,
};

/** Handler for console manager device requests.
 * @param _dev		Device request is being made on.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Positive value on success, negative error code on
 *			failure. */
static int console_manager_request(device_t *manager, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	char name[DEVICE_NAME_MAX];
	console_device_t *console;
	int ret;

	switch(request) {
	case CONSOLE_MANAGER_CREATE:
		if(!outp || !outszp) {
			return -ERR_PARAM_INVAL;
		}

		console = kmalloc(sizeof(console_device_t), MM_SLEEP);
		console->id = atomic_inc(&console_next_id);
		console->mopen = false;
		console->input = pipe_create();
		console->output = pipe_create();

		sprintf(name, "%" PRId32, console->id); 
		if((ret = device_create(name, console_device_dir, NULL, NULL, NULL,
		                        0, &console->dir)) != 0) {
			kfree(console);
			return ret;
		} else if((ret = device_create("master", console->dir, &console_master_ops,
		                               console, NULL, 0, &console->master)) != 0) {
			device_destroy(console->dir);
			kfree(console);
			return ret;
		} else if((ret = device_create("slave", console->dir, &console_slave_ops,
		                               console, NULL, 0, &console->slave)) != 0) {
			device_destroy(console->master);
			device_destroy(console->dir);
			kfree(console);
			return ret;
		}

		return console->id;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Console manager device operations. */
static device_ops_t console_manager_ops = {
	.request = console_manager_request,
};

/** Initialisation function for the console driver.
 * @return		0 on success, negative error code on failure. */
static int console_init(void) {
	int ret;

	/* Create console directory. */
	if((ret = device_create("console", device_tree_root, NULL, NULL, NULL, 0,
	                        &console_device_dir)) != 0) {
		return ret;
	}

	/* Create console manager device. */
	if((ret = device_create("manager", console_device_dir, &console_manager_ops,
	                        NULL, NULL, 0, &console_manager_device)) != 0) {
		device_destroy(console_device_dir);
		return ret;
	}

	return 0;
}

/** Unloading function for the console driver.
 * @return		0 on success, negative error code on failure. */
static int console_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("console");
MODULE_DESC("Console device manager");
MODULE_FUNCS(console_init, console_unload);
