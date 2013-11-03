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
 * @brief		Device manager.
 */

#ifndef __IO_DEVICE_H
#define __IO_DEVICE_H

#include <io/file.h>

#include <kernel/device.h>

#include <lib/radix_tree.h>
#include <lib/refcount.h>

#include <sync/mutex.h>

struct device;

/** Structure containing device operations. */
typedef struct device_ops {
	file_type_t type;			/**< Type of the device. */

	/** Clean up all data associated with a device.
	 * @param device	Device to destroy. */
	void (*destroy)(struct device *device);

	/** Handler for open calls.
	 * @note		Called with device lock held.
	 * @param device	Device being opened.
	 * @param flags		Flags being opened with.
	 * @param datap		Where to store handle-specific data pointer.
	 * @return		Status code describing result of operation. */
	status_t (*open)(struct device *device, uint32_t flags, void **datap);

	/** Handler for close calls.
	 * @note		Called with device lock held.
	 * @param device	Device being released.
	 * @param handle	File handle structure. */
	void (*close)(struct device *device, file_handle_t *handle);

	/** Signal that a device event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param device	Device to wait on.
	 * @param handle	File handle structure.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer to be passed to
	 *			object_wait_signal() or object_wait_notifier().
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct device *device, file_handle_t *handle, unsigned event,
		void *wait);

	/** Stop waiting for a device event.
	 * @param device	Device being waited on.
	 * @param handle	File handle structure.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer. */
	void (*unwait)(struct device *device, file_handle_t *handle, unsigned event,
		void *wait);

	/** Perform I/O on a device.
	 * @param device	Device to perform I/O on.
	 * @param handle	File handle structure.
	 * @param request	I/O request.
	 * @return		Status code describing result of the operation. */
	status_t (*io)(struct device *device, file_handle_t *handle,
		struct io_request *request);

	/** Check if a device can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the device will be classed as mappable if
	 *			get_page is implemented.
	 * @param device	Device being mapped.
	 * @param handle	File handle structure.
	 * @param protection	Protection flags (VM_PROT_*).
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, status code
	 *			explaining why if not. */
	status_t (*mappable)(struct device *device, file_handle_t *handle,
		uint32_t protection, uint32_t flags);

	/** Get a page from the device.
	 * @param device	Device to get page from.
	 * @param handle	File handle structure.
	 * @param offset	Offset into device to get page from.
	 * @param physp		Where to store physical address of page.
	 * @return		Status code describing result of the operation. */
	status_t (*get_page)(struct device *device, file_handle_t *handle,
		offset_t offset, phys_ptr_t *physp);

	/** Handler for device-specific requests.
	 * @param device	Device request is being made on.
	 * @param handle	File handle structure.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param in_size	Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param out_sizep	Where to store output buffer size.
	 * @return		Status code describing result of operation. */
	status_t (*request)(struct device *device, file_handle_t *handle,
		unsigned request, const void *in, size_t in_size, void **outp,
		size_t *out_sizep);
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
	file_t file;				/**< File header. */

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

/** Return values from device_iterate_t. */
enum {
	DEVICE_ITERATE_END,			/**< Finish iteration. */
	DEVICE_ITERATE_DESCEND,			/**< Descend onto children. */
	DEVICE_ITERATE_RETURN,			/**< Return to parent. */
};

/** Device tree iteration callback.
 * @param device	Device currently on.
 * @param data		Iteration data.
 * @return		Action to perform (DEVICE_ITERATE_*). */
typedef int (*device_iterate_t)(device_t *device, void *data);

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

extern status_t device_create(const char *name, device_t *parent,
	device_ops_t *ops, void *data, device_attr_t *attrs, size_t count,
	device_t **devicep);
extern status_t device_alias(const char *name, device_t *parent, device_t *dest,
	device_t **devicep);
extern status_t device_destroy(device_t *device);

extern void device_iterate(device_t *start, device_iterate_t func, void *data);
extern device_attr_t *device_attr(device_t *device, const char *name, int type);
extern char *device_path(device_t *device);

extern status_t device_get(device_t *device, uint32_t rights, uint32_t flags,
	object_handle_t **handlep);
extern status_t device_open(const char *path, uint32_t rights, uint32_t flags,
	object_handle_t **handlep);

extern status_t device_request(object_handle_t *handle, unsigned request,
	const void *in, size_t in_size, void **outp, size_t *out_sizep);

extern void device_init(void);

#endif /* __IO_DEVICE_H */
