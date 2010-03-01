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
 * @brief		Device manager.
 */

#ifndef __IO_DEVICE_H
#define __IO_DEVICE_H

#include <lib/radix.h>
#include <sync/mutex.h>
#include <object.h>

struct device;

/** Structure containing device operations. */
typedef struct device_ops {
	/** Handler for open calls.
	 * @note		Called with device lock held.
	 * @param device	Device being opened.
	 * @param datap		Where to store handle-specific data pointer.
	 * @return		0 on success, negative error code on failure. */
	int (*open)(struct device *device, void **datap);

	/** Handler for close calls.
	 * @note		Called with device lock held.
	 * @param device	Device being released.
	 * @param data		Handle-specific data pointer (all data should
	 *			be freed). */
	void (*close)(struct device *device, void *data);

	/** Read from a device.
	 * @param device	Device to read from.
	 * @param data		Handle specific data pointer.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset to write to (only valid for certain
	 *			device types).
	 * @param bytesp	Where to store number of bytes read.
	 * @return		0 on success, negative error code on failure. */
	int (*read)(struct device *device, void *data, void *buf, size_t count,
	            offset_t offset, size_t *bytesp);

	/** Write to a device.
	 * @param device	Device to write to.
	 * @param data		Handle specific data pointer.
	 * @param buf		Buffer containing data to write.
	 * @param count		Number of bytes to write.
	 * @param offset	Offset to write to (only valid for certain
	 *			device types).
	 * @param bytesp	Where to store number of bytes written.
	 * @return		0 on success, negative error code on failure. */
	int (*write)(struct device *device, void *data, const void *buf, size_t count,
	             offset_t offset, size_t *bytesp);

	/** Signal that a device event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param device	Device to wait for.
	 * @param data		Handle specific data pointer.
	 * @param wait		Wait information structure.
	 * @return		0 on success, negative error code on failure. */
	int (*wait)(struct device *device, void *data, object_wait_t *wait);

	/** Stop waiting for a device event.
	 * @param device	Device to stop waiting for.
	 * @param data		Handle specific data pointer.
	 * @param wait		Wait information structure. */
	void (*unwait)(struct device *device, void *data, object_wait_t *wait);

	/** Fault handler for memory regions mapping the device.
	 * @note		If this operation is not specified then the
	 *			device won't be allowed to be memory-mapped.
	 * @param device	Device fault occurred on.
	 * @param offset	Offset into device fault occurred at (page
	 *			aligned).
	 * @param physp		Where to store address of page to map.
	 * @return		0 on success, negative error code on failure. */
	//int (*fault)(struct device *device, offset_t offset, phys_ptr_t *physp);

	/** Handler for device-specific requests.
	 * @param device	Device request is being made on.
	 * @param data		Handle specific data pointer.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param insz		Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param outszp	Where to store output buffer size.
	 * @return		Positive value on success, negative error code
	 *			on failure. */
	int (*request)(struct device *device, void *data, int request, void *in,
	               size_t insz, void **outp, size_t *outszp);
} device_ops_t;

/** Device attribute structure. */
typedef struct device_attr {
	const char *name;			/**< Attribute name. */

	/** Attribute type. */
	enum {
		DEVICE_ATTR_UINT8,		/**< 8-bit unsigned integer value. */
		DEVICE_ATTR_UINT16,		/**< 16-bit unsigned integer value. */
		DEVICE_ATTR_UINT32,		/**< 32-bit unsigned integer value. */
		DEVICE_ATTR_UINT64,		/**< 64-bit unsigned integer value. */
		DEVICE_ATTR_STRING,		/**< String value. */
	} type;

	/** Attribute value. */
	union {
		uint8_t uint8;			/**< DEVICE_ATTR_UINT8. */
		uint16_t uint16;		/**< DEVICE_ATTR_UINT16. */
		uint32_t uint32;		/**< DEVICE_ATTR_UINT32. */
		uint64_t uint64;		/**< DEVICE_ATTR_UINT64. */
		const char *string;		/**< DEVICE_ATTR_STRING. */
	} value;
} device_attr_t;

/** Structure describing an entry in the device tree. */
typedef struct device {
	object_t obj;				/**< Object header. */

	char *name;				/**< Name of the device. */
	mutex_t lock;				/**< Lock to protect structure. */
	refcount_t count;			/**< Number of users of the device. */

	struct device *parent;			/**< Parent tree entry. */
	radix_tree_t children;			/**< Child devices. */
	struct device *dest;			/**< Destination device if this is an alias. */

	device_ops_t *ops;			/**< Operations structure for the device. */
	void *data;				/**< Data used by the device's creator. */
	device_attr_t *attrs;			/**< Array of attribute structures. */
	size_t attr_count;			/**< Number of attributes. */
} device_t;

/** Device tree iteration callback.
 * @param device	Device currently on.
 * @param data		Iteration data.
 * @return		0 if should finish iteration, 1 if should visit
 *			children, 2 if should return to parent. */
typedef int (*device_iterate_t)(device_t *device, void *data);

/** Various limitations. */
#define DEVICE_NAME_MAX			32	/**< Maximum length of a device name/device attribute name. */
#define DEVICE_ATTR_MAX			256	/**< Maximum length of a device attribute string value. */

/** Generic device events. */
#define DEVICE_EVENT_READABLE		0	/**< Wait for the device to be readable. */
#define DEVICE_EVENT_WRITABLE		1	/**< Wait for the device to be writable. */

/** Start of class-specific event/request numbers. */
#define DEVICE_CLASS_EVENT_START	32
#define DEVICE_CLASS_REQUEST_START	32

/** Start of device-specific event/request numbers. */
#define DEVICE_CUSTOM_EVENT_START	1024
#define DEVICE_CUSTOM_REQUEST_START	1024

extern device_t *device_tree_root;
extern device_t *device_bus_dir;

/** Get the name of a device from a handle.
 * @param handle	Handle to get name from.
 * @return		Name of the device. */
static inline const char *device_name(object_handle_t *handle) {
	device_t *device = (device_t *)handle->object;
	return device->name;
}

extern int device_create(const char *name, device_t *parent, device_ops_t *ops, void *data,
                         device_attr_t *attrs, size_t count, device_t **devicep);
extern int device_alias(const char *name, device_t *parent, device_t *dest, device_t **devicep);
extern int device_destroy(device_t *device);

extern void device_iterate(device_t *start, device_iterate_t func, void *data);
extern int device_lookup(const char *path, device_t **devicep);
extern device_attr_t *device_attr(device_t *device, const char *name, int type);
extern void device_release(device_t *device);

extern int device_open(device_t *device, object_handle_t **handlep);
extern int device_read(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int device_write(object_handle_t *handle, const void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int device_request(object_handle_t *handle, int request, void *in, size_t insz, void **outp, size_t *outszp);

extern int kdbg_cmd_device(int argc, char **argv);

/** Arguments for sys_device_request(). */
typedef struct device_request_args {
	handle_t handle;		/**< Handle to device. */
	int request;			/**< Request number. */
	void *in;			/**< Input buffer. */
	size_t insz;			/**< Input buffer size. */
	void *out;			/**< Output buffer. */
	size_t outsz;			/**< Output buffer size. */
	size_t *bytesp;			/**< Where to store number of bytes returned. */
} device_request_args_t;

extern handle_t sys_device_open(const char *path);
extern int sys_device_read(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int sys_device_write(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int sys_device_request(device_request_args_t *args);

#endif /* __IO_DEVICE_H */
