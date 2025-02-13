/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device manager.
 */

#pragma once

#include <io/file.h>

#include <kernel/device.h>

#include <lib/radix_tree.h>
#include <lib/refcount.h>
#include <lib/utility.h>

#include <sync/mutex.h>
#include <sync/rwlock.h>

#include <module.h>

struct device;
struct device_resource;
struct irq_domain;

/** Structure containing device operations. */
typedef struct device_ops {
    file_type_t type;               /**< Type of the device. */

    /** Clean up all data associated with a device.
     * @param device        Device to destroy. */
    void (*destroy)(struct device *device);

    /** Handler for open calls.
     * @note                Called with device lock held.
     * @param device        Device being opened.
     * @param flags         Flags being opened with.
     * @param _private      Where to store handle-specific private pointer.
     * @return              Status code describing result of operation. */
    status_t (*open)(struct device *device, uint32_t flags, void **_private);

    /** Handler for close calls.
     * @note                Called with device lock held.
     * @param device        Device being released.
     * @param handle        File handle structure. */
    void (*close)(struct device *device, file_handle_t *handle);

    /**
     * Get device size properties. If NULL, size will be set to 0, block_size
     * will be set to 1.
     *
     * @param device        Device to get size of.
     * @param _size         Where to return device size.
     * @param _block_size   Where to return device block size (optimal size for
     *                      I/O operations).
     */
    // TODO: This should eventually be replaced with a device attribute query
    // once the device attribute system is improved.
    void (*size)(struct device *device, offset_t *_size, size_t *_block_size);

    /** Signal that a device event is being waited for.
     * @note                If the event being waited for has occurred
     *                      already, this function should call the callback
     *                      function and return success.
     * @param device        Device to wait on.
     * @param handle        File handle structure.
     * @param event         Event that is being waited for.
     * @return              Status code describing result of the operation. */
    status_t (*wait)(struct device *device, file_handle_t *handle, object_event_t *event);

    /** Stop waiting for a device event.
     * @param device        Device being waited on.
     * @param handle        File handle structure.
     * @param event         Event that is being waited for. */
    void (*unwait)(struct device *device, file_handle_t *handle, object_event_t *event);

    /** Perform I/O on a device.
     * @param device        Device to perform I/O on.
     * @param handle        File handle structure.
     * @param request       I/O request.
     * @return              Status code describing result of the operation. */
    status_t (*io)(struct device *device, file_handle_t *handle, struct io_request *request);

    /** Map a device into memory.
     * @note                See object_type_t::map() for more details on the
     *                      behaviour of this function.
     * @param device        Device to map.
     * @param handle        File handle structure.
     * @param region        Region being mapped.
     * @return              Status code describing result of the operation. */
    status_t (*map)(struct device *device, file_handle_t *handle, struct vm_region *region);

    /** Handler for device-specific requests.
     * @param device        Device request is being made on.
     * @param handle        File handle structure.
     * @param request       Request number.
     * @param in            Input buffer.
     * @param in_size       Input buffer size.
     * @param _out          Where to store pointer to output buffer.
     * @param _out_size     Where to store output buffer size.
     * @return              Status code describing result of operation. */
    status_t (*request)(
        struct device *device, file_handle_t *handle, unsigned request,
        const void *in, size_t in_size, void **_out, size_t *_out_size);
} device_ops_t;

/** Device attribute structure. */
typedef struct device_attr {
    const char *name;               /**< Attribute name. */
    device_attr_type_t type;        /**< Attribute type. */

    /** Attribute value. */
    union {
        int8_t int8;                /**< DEVICE_ATTR_INT8. */
        int16_t int16;              /**< DEVICE_ATTR_INT16. */
        int32_t int32;              /**< DEVICE_ATTR_INT32. */
        int64_t int64;              /**< DEVICE_ATTR_INT64. */
        uint8_t uint8;              /**< DEVICE_ATTR_UINT8. */
        uint16_t uint16;            /**< DEVICE_ATTR_UINT16. */
        uint32_t uint32;            /**< DEVICE_ATTR_UINT32. */
        uint64_t uint64;            /**< DEVICE_ATTR_UINT64. */
        const char *string;         /**< DEVICE_ATTR_STRING. */
    } value;
} device_attr_t;

/** Structure describing an entry in the device tree. */
typedef struct device {
    file_t file;                    /**< File header. */

    char *name;                     /**< Name of the device. */
    mutex_t lock;                   /**< Device lock (covers device tree linkage). */
    refcount_t count;               /**< Number of users of the device. */
    module_t *module;               /**< Module that owns the device. */
    nstime_t time;                  /**< Creation time. */
    uint32_t flags;                 /**< Device flags. */

    /** Device tree linkage. */
    struct device *parent;          /**< Parent tree entry. */
    radix_tree_t children;          /**< Child devices. */
    struct device *dest;            /**< Destination device if this is an alias. */
    union {
        list_t aliases;             /**< Aliases for this device. */
        list_t dest_link;           /**< Link to destination's aliases list. */
    };

    /** Operations. */
    const device_ops_t *ops;        /**< Operations structure for the device. */
    void *private;                  /**< Implementation private data. */

    /** IRQ domain for this device, inherited by any underneath it. */
    struct irq_domain *irq_domain;

    /** Attributes. */
    rwlock_t attr_lock;             /**< Lock for attribute access. */
    device_attr_t *attrs;           /**< Array of attribute structures. */
    size_t attr_count;              /**< Number of attributes. */

    /** Resource management. */
    mutex_t resource_lock;          /**< Lock for resource list. */
    list_t resources;               /**< List of managed resources. */
} device_t;

/** Device flags. */
enum {
    DEVICE_PUBLISHED = (1<<0),      /**< Device is published. */
};

/** Device resource release callback.
 * @param device        Device that the resource is registered to.
 * @param data          Tracking data for resource being released. */
typedef void (*device_resource_release_t)(device_t *device, void *data);

/** Return values from device_iterate_t. */
enum {
    DEVICE_ITERATE_END,             /**< Finish iteration. */
    DEVICE_ITERATE_DESCEND,         /**< Descend onto children. */
    DEVICE_ITERATE_CONTINUE,        /**< Continue iteration without descending. */
};

/** Device tree iteration callback.
 * @param device        Device currently on.
 * @param data          Iteration data.
 * @return              Action to perform (DEVICE_ITERATE_*). */
typedef int (*device_iterate_t)(device_t *device, void *data);

extern device_t *device_root_dir;
extern device_t *device_bus_dir;
extern device_t *device_bus_platform_dir;
extern device_t *device_class_dir;
extern device_t *device_virtual_dir;

extern status_t device_create_etc(
    module_t *module, const char *name, device_t *parent,
    const device_ops_t *ops, void *private, device_attr_t *attrs, size_t count,
    device_t **_device);
extern status_t device_alias_etc(
    module_t *module, const char *name, device_t *parent, device_t *dest,
    device_t **_device);

/** @see device_create_etc(). */
#define device_create(name, parent, ops, private, attrs, count, _device) \
    device_create_etc(module_self(), name, parent, ops, private, attrs, count, _device)

/** Create a device directory.
 * @see device_create_etc(). */
#define device_create_dir(name, parent, _device) \
    device_create_etc(module_self(), name, parent, NULL, NULL, NULL, 0, _device)

/** @see device_alias_etc(). */
#define device_alias(name, parent, dest, _device) \
    device_alias_etc(module_self(), name, parent, dest, _device)

extern void device_set_irq_domain(device_t *device, struct irq_domain *domain);

extern void device_publish(device_t *device);

extern status_t device_destroy(device_t *device);

extern status_t device_attr(
    device_t *device, const char *name, device_attr_type_t type, void *buf,
    size_t size, size_t *_written);

extern void *device_resource_alloc(
    size_t size, device_resource_release_t release, uint32_t mmflag) __malloc;
extern void device_resource_free(void *data);
extern void device_resource_register(device_t *device, void *data);

extern void device_iterate(device_t *start, device_iterate_t func, void *data);
extern char *device_path_inplace(device_t *device, char *buf, size_t size);
extern char *device_path(device_t *device);

extern status_t device_get(
    device_t *device, uint32_t access, uint32_t flags,
    object_handle_t **_handle);
extern status_t device_open(
    const char *path, uint32_t access, uint32_t flags,
    object_handle_t **_handle);

extern device_t *device_from_handle(object_handle_t *handle);

extern int device_kprintf(device_t *device, int level, const char *fmt, ...) __printf(3, 4);

extern void device_early_init(void);
extern void device_init(void);
