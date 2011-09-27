/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Device manager.
 */

#ifndef __IO_DEVICE_H
#define __IO_DEVICE_H

#include <kernel/device.h>
#include <lib/radix_tree.h>
#include <sync/mutex.h>
#include <object.h>

struct device;

/** Structure containing device operations. */
typedef struct device_ops {
	/** Clean up all data associated with a device.
	 * @param device	Device to destroy. */
	void (*destroy)(struct device *device);

	/** Handler for open calls.
	 * @note		Called with device lock held.
	 * @param device	Device being opened.
	 * @param datap		Where to store handle-specific data pointer.
	 * @return		Status code describing result of operation. */
	status_t (*open)(struct device *device, void **datap);

	/** Handler for close calls.
	 * @note		Called with device lock held.
	 * @param device	Device being released.
	 * @param data		Handle-specific data pointer (all data should
	 *			be freed). */
	void (*close)(struct device *device, void *data);

	/** Read from a device.
	 * @param device	Device to read from.
	 * @param data		Handle-specific data pointer.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset to write to (only valid for certain
	 *			device types).
	 * @param bytesp	Where to store number of bytes read.
	 * @return		Status code describing result of operation. */
	status_t (*read)(struct device *device, void *data, void *buf, size_t count,
	                 offset_t offset, size_t *bytesp);

	/** Write to a device.
	 * @param device	Device to write to.
	 * @param data		Handle-specific data pointer.
	 * @param buf		Buffer containing data to write.
	 * @param count		Number of bytes to write.
	 * @param offset	Offset to write to (only valid for certain
	 *			device types).
	 * @param bytesp	Where to store number of bytes written.
	 * @return		Status code describing result of operation. */
	status_t (*write)(struct device *device, void *data, const void *buf, size_t count,
	                  offset_t offset, size_t *bytesp);

	/** Signal that a device event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param device	Device to wait on.
	 * @param data		Handle-specific data pointer.
	 * @param event		Event to wait for.
	 * @param sync		Internal information pointer.
	 * @return		Status code describing result of operation. */
	status_t (*wait)(struct device *device, void *data, int event, void *sync);

	/** Stop waiting for a device event.
	 * @param device	Device to stop waiting for.
	 * @param data		Handle-specific data pointer.
	 * @param event		Event to wait for.
	 * @param sync		Internal information pointer. */
	void (*unwait)(struct device *device, void *data, int event, void *sync);

	/** Check if a device can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the device will be classed as mappable if
	 *			get_page is implemented.
	 * @param device	Device to check.
	 * @param data		Handle-specific data pointer.
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, status code
	 *			explaining why if not. */
	status_t (*mappable)(struct device *device, void *data, int flags);

	/** Get a page for the device (for memory-mapping the device).
	 * @note		See note for mappable.
	 * @param device	Device to get page from.
	 * @param data		Handle-specific data pointer.
	 * @param offset	Offset into device of page to get.
	 * @param physp		Where to store address of page to map.
	 * @return		Status code describing result of operation. */
	status_t (*get_page)(struct device *device, void *data, offset_t offset, phys_ptr_t *physp);

	/** Handler for device-specific requests.
	 * @param device	Device request is being made on.
	 * @param data		Handle-specific data pointer.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param insz		Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param outszp	Where to store output buffer size.
	 * @return		Status code describing result of operation. */
	status_t (*request)(struct device *device, void *data, int request, const void *in,
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
	union {
		list_t aliases;			/**< Aliases for this device. */
		list_t dest_link;		/**< Link to destination's aliases list. */
	};

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

extern status_t device_create(const char *name, device_t *parent, device_ops_t *ops,
                              void *data, device_attr_t *attrs, size_t count,
                              device_t **devicep);
extern status_t device_alias(const char *name, device_t *parent, device_t *dest,
                             device_t **devicep);
extern status_t device_destroy(device_t *device);

extern void device_iterate(device_t *start, device_iterate_t func, void *data);
extern device_attr_t *device_attr(device_t *device, const char *name, int type);
extern char *device_path(device_t *device);

extern status_t device_get(device_t *device, uint32_t rights, object_handle_t **handlep);
extern status_t device_open(const char *path, uint32_t rights, object_handle_t **handlep);
extern status_t device_read(object_handle_t *handle, void *buf, size_t count, offset_t offset,
                            size_t *bytesp);
extern status_t device_write(object_handle_t *handle, const void *buf, size_t count,
                             offset_t offset, size_t *bytesp);
extern status_t device_request(object_handle_t *handle, int request, const void *in, size_t insz,
                               void **outp, size_t *outszp);

extern void device_init(void);

#endif /* __IO_DEVICE_H */
