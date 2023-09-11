/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Device manager.
 */

#include <device/device.h>
#include <device/irq.h>

#include <io/request.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/vm.h>

#include <assert.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** Managed device resource header, allocated together with tracking data. */
typedef struct device_resource {
    list_t header;
    device_resource_release_t release;
    size_t data[];
} device_resource_t;

/** Root of the device tree. */
device_t *device_root_dir;

/**
 * Standard device directories.
 * 
 *  - device_bus_dir ("/bus") - All physical devices in the system live under
 *    this directory, laid out according to how they are connected to the system
 *    (e.g. "/bus/pci/...", "/bus/usb/...").
 *
 *  - device_bus_platform_dir ("/bus/platform") - This is a special bus for
 *    physical devices which exist in the system but not connected to any
 *    specific bus like PCI or USB. For example, hardware blocks built into an
 *    SoC, or legacy PC devices.
 *
 *  - device_class_dir ("/class") - Most devices are of a certain class (e.g.
 *    input, network, etc.), managed by a class driver. Class drivers create
 *    aliases under this directory - everything here should be an alias to
 *    something elsewhere ("/bus" or "/virtual").
 *
 *  - device_virtual_dir("/virtual") - Virtual devices which do not correspond
 *    to a physical device attached to the system.
 */
device_t *device_bus_dir;
device_t *device_bus_platform_dir;
device_t *device_class_dir;
device_t *device_virtual_dir;

/** Open a device. */
static status_t device_file_open(file_handle_t *handle) {
    device_t *device = handle->device;

    if (!module_retain(device->module))
        return STATUS_DEVICE_ERROR;

    status_t ret = STATUS_SUCCESS;
    if (device->ops && device->ops->open)
        ret = device->ops->open(device, file_handle_flags(handle), &handle->private);

    if (ret == STATUS_SUCCESS) {
        refcount_inc(&device->count);
    } else {
        module_release(device->module);
    }

    return ret;
}

/** Close a device. */
static void device_file_close(file_handle_t *handle) {
    if (handle->device->ops && handle->device->ops->close)
        handle->device->ops->close(handle->device, handle);

    module_release(handle->device->module);
    refcount_dec(&handle->device->count);
}

/** Get the name of a device object. */
static char *device_file_name(file_handle_t *handle) {
    char *path = device_path(handle->device);
    if (!path)
        return NULL;

    const char *prefix = "device:";
    size_t path_len    = strlen(path);
    size_t prefix_len  = strlen(prefix);
    size_t size        = path_len + prefix_len + 1;

    char *name = kmalloc(size, MM_KERNEL);
    memcpy(name, prefix, prefix_len);
    memcpy(name + prefix_len, path, path_len);
    name[size - 1] = 0;

    kfree(path);
    return name;
}

/** Get the name of a device object in KDB context. */
static char *device_file_name_unsafe(file_handle_t *handle, char *buf, size_t size) {
    char *path = device_path_inplace(handle->device, buf, size);
    if (!path)
        return NULL;

    const char *prefix      = "device:";
    const size_t prefix_len = strlen(prefix);

    size_t len = strlen(path) + 1;
    if (len + prefix_len <= size) {
        path -= prefix_len;
        memcpy(path, prefix, prefix_len);
    }

    return path;
}

/** Signal that a device event is being waited for. */
static status_t device_file_wait(file_handle_t *handle, object_event_t *event) {
    device_t *device = handle->device;

    if (!device->ops || !device->ops->wait)
        return STATUS_INVALID_EVENT;

    return device->ops->wait(device, handle, event);
}

/** Stop waiting for a device event. */
static void device_file_unwait(file_handle_t *handle, object_event_t *event) {
    device_t *device = handle->device;

    assert(device->ops);
    assert(device->ops->unwait);

    return device->ops->unwait(device, handle, event);
}

/** Perform I/O on a device. */
static status_t device_file_io(file_handle_t *handle, io_request_t *request) {
    device_t *device = handle->device;

    if (!device->ops || !device->ops->io)
        return STATUS_NOT_SUPPORTED;

    return device->ops->io(device, handle, request);
}

/** Map a device into memory.
 * @param handle        File handle structure.
 * @param region        Region being mapped.
 * @return              Status code describing result of the operation. */
static status_t device_file_map(file_handle_t *handle, vm_region_t *region) {
    device_t *device = handle->device;

    /* Cannot create private mappings to devices. */
    if (!device->ops || !device->ops->map || region->flags & VM_MAP_PRIVATE)
        return STATUS_NOT_SUPPORTED;

    return device->ops->map(device, handle, region);
}

/** Get information about a device. */
static void device_file_info(file_handle_t *handle, file_info_t *info) {
    device_t *device = handle->device;

    info->size       = 0;
    info->block_size = 1;

    if (device->ops && device->ops->size)
        device->ops->size(device, &info->size, &info->block_size);

    info->id         = 0;
    info->mount      = 0;
    info->type       = handle->file->type;
    info->links      = 1;
    info->created    = device->time;
    info->accessed   = device->time;
    info->modified   = device->time;
}

/** Handler for device-specific requests. */
static status_t device_file_request(
    file_handle_t *handle, unsigned request, const void *in, size_t in_size,
    void **_out, size_t *_out_size)
{
    device_t *device = handle->device;

    if (!device->ops || !device->ops->request)
        return STATUS_INVALID_REQUEST;

    return device->ops->request(device, handle, request, in, in_size, _out, _out_size);
}

/** Device file operations structure. */
static const file_ops_t device_file_ops = {
    .open        = device_file_open,
    .close       = device_file_close,
    .name        = device_file_name,
    .name_unsafe = device_file_name_unsafe,
    .wait        = device_file_wait,
    .unwait      = device_file_unwait,
    .io          = device_file_io,
    .map         = device_file_map,
    .info        = device_file_info,
    .request     = device_file_request,
};

static void device_ctor(device_t *device) {
    memset(device, 0, sizeof(*device));

    mutex_init(&device->lock, "device_lock", 0);
    refcount_set(&device->count, 0);
    radix_tree_init(&device->children);
    list_init(&device->aliases);
    rwlock_init(&device->attr_lock, "device_attr_lock");
    mutex_init(&device->resource_lock, "device_resource_lock", 0);
    list_init(&device->resources);
}

/**
 * Creates a new node in the device tree. The device created will not have a
 * reference on it. The device can have no operations, in which case it will
 * simply act as a container for other devices.
 *
 * Devices are unpublished when first created. This prevents devices from being
 * used until they have been fully initialized. The device must be published
 * with device_publish() after creation once it is safe for the device to be
 * used.
 *
 * @param module        Module that owns the device.
 * @param name          Name of device to create (will be copied).
 * @param parent        Parent device. Must not be an alias.
 * @param ops           Pointer to operations for the device (can be NULL).
 * @param private       Implementation private data.
 * @param attrs         Optional array of attributes for the device (will be
 *                      duplicated).
 * @param count         Number of attributes.
 * @param _device       Where to store pointer to device structure (can be NULL).
 *
 * @return              Status code describing result of the operation.
 */
status_t device_create_etc(
    module_t *module, const char *name, device_t *parent,
    const device_ops_t *ops, void *private, device_attr_t *attrs, size_t count,
    device_t **_device)
{
    status_t ret;

    assert(module);
    assert(name);
    assert(strlen(name) < DEVICE_NAME_MAX);
    assert(parent);
    assert(!parent->dest);
    assert(!ops || ops->type == FILE_TYPE_BLOCK || ops->type == FILE_TYPE_CHAR);

    mutex_lock(&parent->lock);

    /* Check if a child already exists with this name. */
    if (radix_tree_lookup(&parent->children, name)) {
        ret = STATUS_ALREADY_EXISTS;
        goto out_unlock;
    }

    device_t *device = kmalloc(sizeof(device_t), MM_KERNEL);

    device_ctor(device);

    device->file.ops  = &device_file_ops;
    device->file.type = (ops) ? ops->type : FILE_TYPE_CHAR;
    device->name      = kstrdup(name, MM_KERNEL);
    device->module    = module;
    device->time      = unix_time();
    device->parent    = parent;
    device->ops       = ops;
    device->private   = private;

    /* IRQ domain defaults to that of the parent, can be changed post-init. */
    device->irq_domain = parent->irq_domain;

    if (attrs) {
        /* Ensure the attribute structures are valid. Do validity checking
         * before allocating anything to make it easier to clean up if an
         * invalid structure is found. */
        for (size_t i = 0; i < count; i++) {
            if (!attrs[i].name || strlen(attrs[i].name) >= DEVICE_NAME_MAX) {
                ret = STATUS_INVALID_ARG;
                goto err_free;
            } else if (attrs[i].type == DEVICE_ATTR_STRING) {
                if (!attrs[i].value.string || strlen(attrs[i].value.string) >= DEVICE_ATTR_MAX) {
                    ret = STATUS_INVALID_ARG;
                    goto err_free;
                }
            }
        }

        /* Duplicate the structures, then fix up the data. */
        device->attrs      = kmemdup(attrs, sizeof(device_attr_t) * count, MM_KERNEL);
        device->attr_count = count;

        for (size_t i = 0; i < device->attr_count; i++) {
            device->attrs[i].name = kstrdup(device->attrs[i].name, MM_KERNEL);

            if (device->attrs[i].type == DEVICE_ATTR_STRING)
                device->attrs[i].value.string = kstrdup(device->attrs[i].value.string, MM_KERNEL);
        }
    } else {
        device->attrs      = NULL;
        device->attr_count = 0;
    }

    /* Attach to the parent. */
    refcount_inc(&parent->count);
    radix_tree_insert(&parent->children, device->name, device);

    kprintf(LOG_DEBUG, "device: created device %pD (module: %s)\n", device, module->name);

    if (_device)
        *_device = device;

    ret = STATUS_SUCCESS;

out_unlock:
    mutex_unlock(&parent->lock);
    return ret;

err_free:
    kfree(device->name);
    kfree(device);
    goto out_unlock;
}

/**
 * Creates an alias for another device in the device tree. Any attempts to open
 * the alias will open the device it is an alias for.
 *
 * @param module        Module that owns the device.
 * @param name          Name to give alias (will be copied).
 * @param parent        Device to create alias under.
 * @param dest          Destination device. If this is an alias, the new alias
 *                      will refer to the destination, not the alias itself.
 * @param _device       Where to store pointer to alias structure (can be NULL).
 *
 * @return              Status code describing result of the operation.
 */
status_t device_alias_etc(
    module_t *module, const char *name, device_t *parent, device_t *dest,
    device_t **_device)
{
    assert(module);
    assert(name);
    assert(strlen(name) < DEVICE_NAME_MAX);
    assert(parent);
    assert(!parent->dest);
    assert(dest);

    /* If the destination is an alias, use it's destination. */
    if (dest->dest)
        dest = dest->dest;

    mutex_lock(&parent->lock);

    /* Check if a child already exists with this name. */
    if (radix_tree_lookup(&parent->children, name)) {
        mutex_unlock(&parent->lock);
        return STATUS_ALREADY_EXISTS;
    }

    device_t *device = kmalloc(sizeof(device_t), MM_KERNEL);

    device_ctor(device);

    device->file.ops   = &device_file_ops;
    device->file.type  = (dest->ops) ? dest->ops->type : FILE_TYPE_CHAR;
    device->name       = kstrdup(name, MM_KERNEL);
    device->module     = module;
    device->time       = dest->time;
    device->parent     = parent;
    device->dest       = dest;

    /* Aliases are published, but whether they are actually available depends
     * on whether the destination is published. */
    device->flags |= DEVICE_PUBLISHED;

    refcount_inc(&parent->count);
    radix_tree_insert(&parent->children, device->name, device);

    mutex_unlock(&parent->lock);

    /* Add the device to the destination's alias list. */
    mutex_lock(&dest->lock);
    list_append(&dest->aliases, &device->dest_link);
    mutex_unlock(&dest->lock);

    kprintf(LOG_DEBUG, "device: created alias %pD to %pD\n", device, dest);

    if (_device)
        *_device = device;

    return STATUS_SUCCESS;
}

/**
 * Sets the IRQ domain for a device. This should generally only be used by bus
 * managers, immediately after creating the device. It must not be used on
 * devices that already have children - creating a child copies the domain from
 * the parent so changes would not propagate down to children.
*/
void device_set_irq_domain(device_t *device, irq_domain_t *domain) {
    assert(radix_tree_empty(&device->children));

    device->irq_domain = domain;
}

/**
 * Publishes a device. This makes the device, and any published child devices,
 * available for use.
 *
 * @param device        Device to publish.
 */
void device_publish(device_t *device) {
    mutex_lock(&device->lock);
    device->flags |= DEVICE_PUBLISHED;
    mutex_unlock(&device->lock);
}

/**
 * Removes a device from the device tree. The device must have no users. All
 * aliases of the device will be removed.
 *
 * @todo                Sometime we'll need to allow devices to be removed when
 *                      they have users, for example for hotplugging.
 * @fixme               I don't think alias removal is entirely thread-safe.
 *
 * @param device        Device to remove.
 *
 * @return              Status code describing result of the operation. Cannot
 *                      fail if the device being removed is an alias.
 */
status_t device_destroy(device_t *device) {
    assert(device->parent);

    mutex_lock(&device->parent->lock);
    mutex_lock(&device->lock);

    if (refcount_get(&device->count) != 0) {
        mutex_unlock(&device->lock);
        mutex_unlock(&device->parent->lock);
        return STATUS_IN_USE;
    }

    device->flags &= ~DEVICE_PUBLISHED;

    /* Call the device's destroy operation, if any. */
    if (device->ops && device->ops->destroy)
        device->ops->destroy(device);

    /* Release managed resources. Do this in reverse so we release in reverse
     * order to what they were registered in. */
    while (!list_empty(&device->resources)) {
        device_resource_t *resource = list_last(&device->resources, device_resource_t, header);
        list_remove(&resource->header);

        if (resource->release)
            resource->release(device, resource->data);

        kfree(resource);
    }

    /* Remove all aliases to the device. */
    if (!device->dest) {
        list_foreach(&device->aliases, iter) {
            device_t *alias = list_entry(iter, device_t, dest_link);
            device_destroy(alias);
        }
    }

    radix_tree_remove(&device->parent->children, device->name, NULL);
    refcount_dec(&device->parent->count);

    mutex_unlock(&device->parent->lock);
    mutex_unlock(&device->lock);

    kprintf(LOG_DEBUG, "device: destroyed device %s\n", device->name);

    /* Free up attributes if any. */
    if (device->attrs) {
        for (size_t i = 0; i < device->attr_count; i++) {
            kfree((char *)device->attrs[i].name);
            if (device->attrs[i].type == DEVICE_ATTR_STRING)
                kfree((char *)device->attrs[i].value.string);
        }

        kfree(device->attrs);
    }

    kfree(device->name);
    kfree(device);

    return STATUS_SUCCESS;
}

/** Gets the value of a device attribute.
 * @param device        Device to get attribute from.
 * @param name          Name of attribute to get.
 * @param type          Expected type of the attribute. An error will be
 *                      returned if the attribute is not this type.
 * @param buf           Buffer to write attribute value to.
 * @param size          Size of buffer. For integer types, this must be the
 *                      exact size of the type. For strings, this is the maximum
 *                      buffer size, and if the buffer cannot fit the null-
 *                      terminated attribute value, STATUS_TOO_SMALL will be
 *                      returned. String attributes can be no longer than
 *                      DEVICE_ATTR_MAX (including null terminator).
 * @param _written      Where to store actual length of attribute value.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if type is an integer and size is not
 *                      the exact size of that type.
 *                      STATUS_NOT_FOUND if attribute is not found.
 *                      STATUS_INCORRECT_TYPE if attribute is not the expected
 *                      type.
 *                      STATUS_TOO_SMALL if size cannot accomodate the attribute
 *                      value.
 */
status_t device_attr(
    device_t *device, const char *name, device_attr_type_t type, void *buf,
    size_t size, size_t *_written)
{
    assert(device);
    assert(name);
    assert(buf);

    if (_written)
        *_written = 0;

    size_t expected_size = 0;
    switch (type) {
        case DEVICE_ATTR_INT8:  case DEVICE_ATTR_UINT8:  expected_size = 1; break;
        case DEVICE_ATTR_INT16: case DEVICE_ATTR_UINT16: expected_size = 2; break;
        case DEVICE_ATTR_INT32: case DEVICE_ATTR_UINT32: expected_size = 4; break;
        case DEVICE_ATTR_INT64: case DEVICE_ATTR_UINT64: expected_size = 8; break;
        default: break;
    }

    if (expected_size > 0 && size != expected_size)
        return STATUS_INVALID_ARG;

    rwlock_read_lock(&device->attr_lock);

    status_t ret = STATUS_NOT_FOUND;
    for (size_t i = 0; i < device->attr_count; i++) {
        device_attr_t *attr = &device->attrs[i];

        if (strcmp(attr->name, name) == 0) {
            if (attr->type == type) {
                ret = STATUS_SUCCESS;

                switch (type) {
                    case DEVICE_ATTR_INT8:
                        *(int8_t *)buf = attr->value.int8;
                        break;
                    case DEVICE_ATTR_INT16:
                        *(int16_t *)buf = attr->value.int16;
                        break;
                    case DEVICE_ATTR_INT32:
                        *(int32_t *)buf = attr->value.int32;
                        break;
                    case DEVICE_ATTR_INT64:
                        *(int64_t *)buf = attr->value.int64;
                        break;
                    case DEVICE_ATTR_UINT8:
                        *(uint8_t *)buf = attr->value.uint8;
                        break;
                    case DEVICE_ATTR_UINT16:
                        *(uint16_t *)buf = attr->value.uint16;
                        break;
                    case DEVICE_ATTR_UINT32:
                        *(uint32_t *)buf = attr->value.uint32;
                        break;
                    case DEVICE_ATTR_UINT64:
                        *(uint64_t *)buf = attr->value.uint64;
                        break;
                    case DEVICE_ATTR_STRING:
                        expected_size = strlen(attr->value.string) + 1;
                        if (expected_size <= size) {
                            memcpy(buf, attr->value.string, expected_size);
                        } else {
                            ret = STATUS_TOO_SMALL;
                        }

                        break;
                }
            } else {
                ret = STATUS_INCORRECT_TYPE;
            }

            break;
        }
    }

    if (ret == STATUS_SUCCESS && _written)
        *_written = expected_size;

    rwlock_unlock(&device->attr_lock);
    return ret;
}

/**
 * Allocates a structure for tracking a device managed resource. This structure
 * should contain everything needed to be able to release the resource later
 * on. Internally, it is allocated inside another structure.
 *
 * @param size          Size of the tracking structure.
 * @param release       Callback function to release the resource (can be null).
 * @param mmflag        Allocation flags.
 *
 * @return              Allocated structure, or null on failure (if mmflag
 *                      allows it).
 */
void *device_resource_alloc(size_t size, device_resource_release_t release, uint32_t mmflag) {
    assert(release);

    device_resource_t *resource = kmalloc(sizeof(device_resource_t) + size, mmflag);
    if (!resource)
        return NULL;

    list_init(&resource->header);
    resource->release = release;

    return resource->data;
}

/**
 * Frees a device resource tracking structure. Only needs to be used if the
 * structure needs to be freed due to a failure before it is passed to
 * device_resource_register().
 *
 * @param data          Resource tracking structure.
 */
void device_resource_free(void *data) {
    device_resource_t *resource = container_of(data, device_resource_t, data);

    assert(list_empty(&resource->header));

    kfree(resource);
}

/**
 * Registers a resource with a device, such that it will be released when the
 * device is destroyed. Once a tracking structure is passed to this function,
 * the caller no longer owns it and should not alter or free it.
 *
 * When a device is destroyed, resources are released in reverse order to what
 * they were registered in.
 *
 * @param device        Device to register with.
 * @param data          Resource tracking structure.
 */
void device_resource_register(device_t *device, void *data) {
    device_resource_t *resource = container_of(data, device_resource_t, data);

    mutex_lock(&device->resource_lock);
    list_append(&device->resources, &resource->header);
    mutex_unlock(&device->resource_lock);
}

static bool device_iterate_internal(device_t *device, device_iterate_t func, void *data) {
    while (device->dest)
        device = device->dest;

    switch (func(device, data)) {
        case DEVICE_ITERATE_END:
            return false;
        case DEVICE_ITERATE_DESCEND:
            radix_tree_foreach(&device->children, iter) {
                device_t *child = radix_tree_entry(iter, device_t);

                if (!device_iterate_internal(child, func, data))
                    return false;
            }
        case DEVICE_ITERATE_CONTINUE:
            return true;
    }

    return false;
}

/**
 * Iterates through the device tree. The specified function will be called on a
 * device and all its children (and all their children, etc).
 *
 * @todo                This function is really unsafe since it doesn't do any
 *                      locking or reference counting...
 *
 * @param start         Starting device.
 * @param func          Function to call on devices.
 * @param data          Data argument to pass to function.
 */
void device_iterate(device_t *start, device_iterate_t func, void *data) {
    /* TODO: We have small kernel stacks. Recursive lookup probably isn't a
     * very good idea. Then again, the device tree shouldn't go *too* deep. */
    device_iterate_internal(start, func, data);
}

/** Test if a device is effectively published (i.e. including all its parents). */
static bool device_is_published(device_t *device) {
    while (device) {
        if (!(device->flags & DEVICE_PUBLISHED))
            return false;

        device = device->parent;
    }

    return true;
}

/** Looks up a device and increase its reference count.
 * @param path          Path to device.
 * @return              Pointer to device if found, NULL if not. */
static device_t *device_lookup(const char *path) {
    assert(path);

    if (!path[0] || path[0] != '/')
        return NULL;

    char *dup = kstrdup(path, MM_KERNEL);
    char *orig __cleanup_kfree __unused = dup;

    device_t *device = device_root_dir;
    mutex_lock(&device->lock);

    char *tok;
    while ((tok = strsep(&dup, "/"))) {
        if (!tok[0])
            continue;

        device_t *child = radix_tree_lookup(&device->children, tok);
        if (!child) {
            mutex_unlock(&device->lock);
            return NULL;
        }

        /* Move down to the device. */
        mutex_lock(&child->lock);
        mutex_unlock(&device->lock);
        device = child;

        /* If this is an alias, go to the destination. This is guaranteed (by
         * device_alias()) to not be another alias. */
        if (device->dest) {
            child = device->dest;
            mutex_lock(&child->lock);
            mutex_unlock(&device->lock);
            device = child;

            /* We must retest if the destination is actually published from
             * the root (all parents must be published for it to be available),
             * since we have not gone through the full tree to get to the
             * destination. */
            if (!device_is_published(device)) {
                mutex_unlock(&device->lock);
                return NULL;
            }
        } else if (!(device->flags & DEVICE_PUBLISHED)) {
            mutex_unlock(&device->lock);
            return NULL;
        }
    }

    refcount_inc(&device->count);
    mutex_unlock(&device->lock);

    return device;
}

/**
 * Constructs a device path string in-place in a buffer. It will be constructed
 * backwards and a pointer to the start of the string will be returned.
 *
 * @param device        Device to get path to. Must be guaranteed to be alive
 *                      through the call (e.g. caller holds a reference).
 * @param buf           Buffer to construct into.
 * @param size          Size of the buffer.
 *
 * @return              Pointer to start of string.
 */
char *device_path_inplace(device_t *device, char *buf, size_t size) {
    assert(size > 0);

    /* Build device path backwards. No need to lock devices, names are immutable
     * and since we require the device to remain alive across the function call,
     * the tree linkage cannot change either. */
    char *path = &buf[size - 1];
    *path = 0;
    size_t len = 0;

    device_t *iter = device;
    while (iter != device_root_dir) {
        size_t name_len = strlen(iter->name);

        len += name_len + 1;
        if (len > size)
            break;

        path -= name_len;
        memcpy(path, iter->name, name_len);

        path--;
        *path = '/';

        iter = iter->parent;
    }

    if (len == 0) {
        path--;
        *path = '/';
    }

    return path;
}

/** Gets the path to a device.
 * @param device        Device to get path to.
 * @return              Pointer to kmalloc()'d string containing device path. */
char *device_path(device_t *device) {
    char *path = NULL;
    size_t len = 0;

    while (device != device_root_dir) {
        mutex_lock(&device->lock);

        len += strlen(device->name) + 1;

        char *tmp = kmalloc(len + 1, MM_KERNEL);
        strcpy(tmp, "/");
        strcat(tmp, device->name);
        if (path) {
            strcat(tmp, path);
            kfree(path);
        }

        path = tmp;

        device_t *parent = device->parent;
        mutex_unlock(&device->lock);
        device = parent;
    }

    if (!path)
        path = kstrdup("/", MM_KERNEL);

    return path;
}


/** Creates a handle to a device.
 * @param device        Device to get handle to.
 * @param access        Requested access rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store handle to device.
 * @return              Status code describing result of the operation. */
status_t device_get(device_t *device, uint32_t access, uint32_t flags, object_handle_t **_handle) {
    status_t ret;

    assert(device);
    assert(_handle);

    if (!module_retain(device->module))
        return STATUS_DEVICE_ERROR;

    mutex_lock(&device->lock);

    if (!device_is_published(device)) {
        ret = STATUS_NOT_FOUND;
        goto err;
    } else if (access && !file_access(&device->file, access)) {
        ret = STATUS_ACCESS_DENIED;
        goto err;
    }

    file_handle_t *handle = file_handle_alloc(&device->file, access, flags);

    if (device->ops && device->ops->open) {
        ret = device->ops->open(device, flags, &handle->private);
        if (ret != STATUS_SUCCESS) {
            file_handle_free(handle);
            goto err;
        }
    }

    refcount_inc(&device->count);
    *_handle = file_handle_create(handle);
    mutex_unlock(&device->lock);

    return STATUS_SUCCESS;

err:
    mutex_unlock(&device->lock);
    module_release(device->module);
    return ret;
}

/** Creates a handle to a device.
 * @param path          Path to device to open.
 * @param access        Requested access access for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store pointer to handle structure.
 * @return              Status code describing result of the operation. */
status_t device_open(const char *path, uint32_t access, uint32_t flags, object_handle_t **_handle) {
    status_t ret;

    assert(path);
    assert(_handle);

    device_t *device = device_lookup(path);
    if (!device)
        return STATUS_NOT_FOUND;

    if (!module_retain(device->module)) {
        ret = STATUS_DEVICE_ERROR;
        goto err_release;
    }

    mutex_lock(&device->lock);

    if (access && !file_access(&device->file, access)) {
        ret = STATUS_ACCESS_DENIED;
        goto err_unlock;
    }

    file_handle_t *handle = file_handle_alloc(&device->file, access, flags);

    if (device->ops && device->ops->open) {
        ret = device->ops->open(device, flags, &handle->private);
        if (ret != STATUS_SUCCESS) {
            file_handle_free(handle);
            goto err_unlock;
        }
    }

    *_handle = file_handle_create(handle);
    mutex_unlock(&device->lock);

    return STATUS_SUCCESS;

err_unlock:
    mutex_unlock(&device->lock);
    module_release(device->module);

err_release:
    refcount_dec(&device->count);
    return ret;
}

/**
 * Gets the underlying device from a handle. This is only safe to use while a
 * reference is still held to the handle.
 *
 * @param handle        Handle to get device for.
 *
 * @return              Pointer to device, or NULL if the handle is not a device
 *                      handle.
 */
device_t *device_from_handle(object_handle_t *_handle) {
    if (_handle->type->id != OBJECT_TYPE_FILE)
        return NULL;

    file_handle_t *handle = _handle->private;

    if (handle->file->ops != &device_file_ops)
        return NULL;

    return handle->device;
}

/**
 * Device-specific version of kprintf() which will prefix messages with the
 * device module name and path. It is assumed that the device will not be
 * destroyed for the duration of the function.
 * 
 * @param device        Device to log for.
 * @param level         Log level.
 * @param fmt           Format string for message.
 * @param ...           Arguments to substitute into format string.
 *
 * @return              Number of characters written.
 */
int device_kprintf(device_t *device, int level, const char *fmt, ...) {
    int ret = kprintf(level, "%s: %pD: ", device->module->name, device);

    va_list args;
    va_start(args, fmt);

    ret += kvprintf(level, fmt, args);

    va_end(args);
    return ret;
}

/** Device path buffer to avoid using stack or dynamic allocation. */
static char kdb_device_path_buf[DEVICE_PATH_MAX];

static void dump_children(radix_tree_t *tree, int indent) {
    radix_tree_foreach(tree, iter) {
        device_t *device = radix_tree_entry(iter, device_t);

        const char *dest = (device->dest)
            ? device_path_inplace(device->dest, kdb_device_path_buf, DEVICE_PATH_MAX)
            : "<none>";

        kdb_printf(
            "%*s%-*s %-18p %-16s %c    %-6d %s\n", indent, "",
            32 - indent, device->name, device, device->module->name,
            (device->flags & DEVICE_PUBLISHED) ? 'Y' : 'N',
            refcount_get(&device->count), dest);

        if (!device->dest)
            dump_children(&device->children, indent + 2);
    }
}

static kdb_status_t kdb_cmd_device(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<addr>]\n\n", argv[0]);

        kdb_printf("If no arguments are given, shows the contents of the device tree. Otherwise\n");
        kdb_printf("shows information about a single device.\n");
        return KDB_SUCCESS;
    } else if (argc != 1 && argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    if (argc == 1) {
        kdb_printf("Name                             Address            Module           Pub  Count  Destination\n");
        kdb_printf("====                             =======            ======           ===  =====  ===========\n");

        dump_children(&device_root_dir->children, 0);
        return KDB_SUCCESS;
    }

    uint64_t val;
    if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    device_t *device = (device_t *)((ptr_t)val);

    const char *path = device_path_inplace(device, kdb_device_path_buf, DEVICE_PATH_MAX);

    kdb_printf("Device %p \"%s\"\n", device, path);
    kdb_printf("=================================================\n");
    kdb_printf("Count:       %d\n", refcount_get(&device->count));
    kdb_printf("Parent:      %p\n", device->parent);

    if (device->dest) {
        const char *dest = device_path_inplace(device->dest, kdb_device_path_buf, DEVICE_PATH_MAX);
        kdb_printf("Destination: %p \"%s\"\n", device->dest, dest);
    }
    kdb_printf("Module:      %s\n", device->module->name);
    kdb_printf("Ops:         %p\n", device->ops);
    kdb_printf("Private:     %p\n", device->private);
    kdb_printf("Flags:       0x%" PRIx32 "\n", device->flags);

    if (!device->attrs)
        return KDB_SUCCESS;

    kdb_printf("\nAttributes:\n");

    for (size_t i = 0; i < device->attr_count; i++) {
        kdb_printf("  %s - ", device->attrs[i].name);
        switch (device->attrs[i].type) {
            case DEVICE_ATTR_INT8:
                kdb_printf(
                    "int8: %" PRId8 " (0x%" PRIx8 ")\n",
                    device->attrs[i].value.int8, device->attrs[i].value.int8);
                break;
            case DEVICE_ATTR_INT16:
                kdb_printf(
                    "int16: %" PRId16 " (0x%" PRIx16 ")\n",
                    device->attrs[i].value.int16, device->attrs[i].value.int16);
                break;
            case DEVICE_ATTR_INT32:
                kdb_printf(
                    "int32: %" PRId32 " (0x%" PRIx32 ")\n",
                    device->attrs[i].value.int32, device->attrs[i].value.int32);
                break;
            case DEVICE_ATTR_INT64:
                kdb_printf(
                    "int64: %" PRId64 " (0x%" PRIx64 ")\n",
                    device->attrs[i].value.int64, device->attrs[i].value.int64);
                break;
            case DEVICE_ATTR_UINT8:
                kdb_printf(
                    "uint8: %" PRIu8 " (0x%" PRIx8 ")\n",
                    device->attrs[i].value.uint8, device->attrs[i].value.uint8);
                break;
            case DEVICE_ATTR_UINT16:
                kdb_printf(
                    "uint16: %" PRIu16 " (0x%" PRIx16 ")\n",
                    device->attrs[i].value.uint16, device->attrs[i].value.uint16);
                break;
            case DEVICE_ATTR_UINT32:
                kdb_printf(
                    "uint32: %" PRIu32 " (0x%" PRIx32 ")\n",
                    device->attrs[i].value.uint32, device->attrs[i].value.uint32);
                break;
            case DEVICE_ATTR_UINT64:
                kdb_printf(
                    "uint64: %" PRIu64 " (0x%" PRIx64 ")\n",
                    device->attrs[i].value.uint64, device->attrs[i].value.uint64);
                break;
            case DEVICE_ATTR_STRING:
                kdb_printf("string: '%s'\n", device->attrs[i].value.string);
                break;
            default:
                kdb_printf("Invalid!\n");
                break;
        }
    }

    return KDB_SUCCESS;
}

static status_t null_device_io(device_t *device, file_handle_t *handle, io_request_t *request) {
    if (request->op == IO_OP_WRITE) {
        request->transferred = request->total;
    } else {
        request->transferred = 0;
    }

    return STATUS_SUCCESS;
}

static const device_ops_t null_device_ops = {
    .type = FILE_TYPE_CHAR,
    .io   = null_device_io,
};

static void null_device_init(void) {
    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = "null" } },
    };

    device_t *device;
    status_t ret = device_create(
        "null", device_virtual_dir, &null_device_ops, NULL, attrs,
        array_size(attrs), &device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to register null device (%d)", ret);

    device_publish(device);
}

/** Early device initialization. */
__init_text void device_early_init(void) {
    initcall_run(INITCALL_TYPE_EARLY_DEVICE);
}

/** Initialize the device manager. */
__init_text void device_init(void) {
    status_t ret;

    /* Create the root node of the device tree. */
    device_root_dir = kmalloc(sizeof(device_t), MM_BOOT);

    device_ctor(device_root_dir);

    device_root_dir->file.ops   = &device_file_ops;
    device_root_dir->file.type  = FILE_TYPE_CHAR;
    device_root_dir->name       = (char *)"<root>";
    device_root_dir->time       = boot_time();
    device_root_dir->module     = &kernel_module;
    device_root_dir->irq_domain = root_irq_domain;

    /* Create standard device directories. */
    ret = device_create_dir("bus", device_root_dir, &device_bus_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create_dir("platform", device_bus_dir, &device_bus_platform_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create_dir("class", device_root_dir, &device_class_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create_dir("virtual", device_root_dir, &device_virtual_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    device_publish(device_root_dir);
    device_publish(device_bus_dir);
    device_publish(device_bus_platform_dir);
    device_publish(device_class_dir);
    device_publish(device_virtual_dir);

    null_device_init();

    kdb_register_command("device", "Examine the device tree.", kdb_cmd_device);
}

/** Opens a handle to a device.
 * @param path          Device tree path for device to open.
 * @param access        Requested access rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store handle to the device.
 * @return              Status code describing result of the operation. */
status_t kern_device_open(const char *path, uint32_t access, uint32_t flags, handle_t *_handle) {
    status_t ret;

    if (!path || !_handle)
        return STATUS_INVALID_ARG;

    char *kpath;
    ret = strndup_from_user(path, DEVICE_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *handle;
    ret = device_open(kpath, access, flags, &handle);
    if (ret != STATUS_SUCCESS) {
        kfree(kpath);
        return ret;
    }

    ret = object_handle_attach(handle, NULL, _handle);
    object_handle_release(handle);
    kfree(kpath);
    return ret;
}

/** Gets the value of a device attribute.
 * @param handle        Handle to device.
 * @param name          Name of attribute to get.
 * @param type          Expected type of the attribute. An error will be
 *                      returned if the attribute is not this type.
 * @param buf           Buffer to write attribute value to.
 * @param size          Size of buffer. For integer types, this must be the
 *                      exact size of the type. For strings, this is the maximum
 *                      buffer size, and if the buffer cannot fit the null-
 *                      terminated attribute value, STATUS_TOO_SMALL will be
 *                      returned. String attributes can be no longer than
 *                      DEVICE_ATTR_MAX (including null terminator).
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if type is an integer and size is not
 *                      the exact size of that type.
 *                      STATUS_NOT_FOUND if attribute is not found.
 *                      STATUS_INCORRECT_TYPE if attribute is not the expected
 *                      type.
 *                      STATUS_TOO_SMALL if size cannot accomodate the attribute
 *                      value. */
status_t kern_device_attr(handle_t handle, const char* name, device_attr_type_t type, void *buf, size_t size) {
    status_t ret;

    if (!name || !buf || size > DEVICE_ATTR_MAX)
        return STATUS_INVALID_ARG;

    char *kname;
    ret = strndup_from_user(name, DEVICE_NAME_MAX, &kname);
    if (ret != STATUS_SUCCESS)
        return ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS) {
        kfree(kname);
        return ret;
    }

    file_handle_t *fhandle = khandle->private;

    if (fhandle->file->ops == &device_file_ops) {
        void *kbuf = kmalloc(size, MM_KERNEL);

        size_t written;
        ret = device_attr(fhandle->device, kname, type, kbuf, size, &written);

        if (ret == STATUS_SUCCESS)
            ret = memcpy_to_user(buf, kbuf, written);

        kfree(kbuf);
    } else {
        ret = STATUS_NOT_SUPPORTED;
    }

    object_handle_release(khandle);
    kfree(kname);
    return ret;
}
