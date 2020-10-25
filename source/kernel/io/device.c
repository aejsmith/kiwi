/*
 * Copyright (C) 2009-2020 Alex Smith
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

#include <io/device.h>
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
        ret = device->ops->open(device, handle->flags, &handle->private);

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

    info->id         = 0;
    info->mount      = 0;
    info->type       = handle->file->type;
    info->block_size = 0;
    info->size       = 0;
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
static file_ops_t device_file_ops = {
    .open    = device_file_open,
    .close   = device_file_close,
    .wait    = device_file_wait,
    .unwait  = device_file_unwait,
    .io      = device_file_io,
    .map     = device_file_map,
    .info    = device_file_info,
    .request = device_file_request,
};

/**
 * Creates a new node in the device tree. The device created will not have a
 * reference on it. The device can have no operations, in which case it will
 * simply act as a container for other devices.
 *
 * @param module        Module that owns the device.
 * @param name          Name of device to create (will be copied).
 * @param parent        Parent device. Must not be an alias.
 * @param ops           Pointer to operations for the device (can be NULL).
 * @param data          Implementation-specific data pointer.
 * @param attrs         Optional array of attributes for the device (will be
 *                      duplicated).
 * @param count         Number of attributes.
 * @param _device       Where to store pointer to device structure (can be NULL).
 *
 * @return              Status code describing result of the operation.
 */
status_t device_create_impl(
    module_t *module, const char *name, device_t *parent, device_ops_t *ops,
    void *data, device_attr_t *attrs, size_t count, device_t **_device)
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

    mutex_init(&device->lock, "device_lock", 0);
    refcount_set(&device->count, 0);
    radix_tree_init(&device->children);
    list_init(&device->aliases);

    device->file.ops  = &device_file_ops;
    device->file.type = (ops) ? ops->type : FILE_TYPE_CHAR;
    device->name      = kstrdup(name, MM_KERNEL);
    device->module    = module;
    device->time      = system_time();
    device->parent    = parent;
    device->dest      = NULL;
    device->ops       = ops;
    device->data      = data;

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

    kprintf(
        LOG_DEBUG, "device: created device %s in %s (ops: %p, data: %p)\n",
        device->name, parent->name, ops, data);

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
status_t device_alias_impl(
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

    mutex_init(&device->lock, "device_alias_lock", 0);
    refcount_set(&device->count, 0);
    radix_tree_init(&device->children);
    list_init(&device->dest_link);

    device->file.ops   = &device_file_ops;
    device->file.type  = (dest->ops) ? dest->ops->type : FILE_TYPE_CHAR;
    device->name       = kstrdup(name, MM_KERNEL);
    device->module     = module;
    device->time       = dest->time;
    device->parent     = parent;
    device->dest       = dest;
    device->ops        = NULL;
    device->data       = NULL;
    device->attrs      = NULL;
    device->attr_count = 0;

    refcount_inc(&parent->count);
    radix_tree_insert(&parent->children, device->name, device);

    mutex_unlock(&parent->lock);

    /* Add the device to the destination's alias list. */
    mutex_lock(&dest->lock);
    list_append(&dest->aliases, &device->dest_link);
    mutex_unlock(&dest->lock);

    kprintf(
        LOG_DEBUG, "device: created alias %s in %s to %s\n",
        device->name, parent->name, dest->name);

    if (_device)
        *_device = device;

    return STATUS_SUCCESS;
}

/**
 * Removes a device from the device tree. The device must have no users. All
 * aliases of the device should be destroyed before the device itself.
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

    /* Call the device's destroy operation, if any. */
    if (device->ops && device->ops->destroy)
        device->ops->destroy(device);

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

static bool device_iterate_internal(device_t *device, device_iterate_t func, void *data) {
    switch (func(device, data)) {
        case DEVICE_ITERATE_END:
            return false;
        case DEVICE_ITERATE_DESCEND:
            radix_tree_foreach(&device->children, iter) {
                device_t *child = radix_tree_entry(iter, device_t);

                if (!device_iterate_internal(child, func, data))
                    return false;
            }
        case DEVICE_ITERATE_RETURN:
            return true;
    }

    return false;
}

/**
 * Iterates through the device tree. The specified function will be called on a
 * device and all its children (and all their children, etc).
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

/** Look up a device and increase its reference count.
 * @param path          Path to device.
 * @return              Pointer to device if found, NULL if not. */
static device_t *device_lookup(const char *path) {
    assert(path);

    if (!path[0] || path[0] != '/')
        return NULL;

    char *dup  = kstrdup(path, MM_KERNEL);
    char *orig = dup;

    device_t *device = device_root_dir;
    mutex_lock(&device->lock);

    char *tok;
    while ((tok = strsep(&dup, "/"))) {
        if (!tok[0])
            continue;

        device_t *child = radix_tree_lookup(&device->children, tok);
        if (!child) {
            mutex_unlock(&device->lock);
            kfree(orig);
            return NULL;
        }

        /* Move down to the device and then iterate through until we reach an
         * entry that isn't an alias. */
        do {
            mutex_lock(&child->lock);
            mutex_unlock(&device->lock);
            device = child;
        } while ((child = device->dest));
    }

    refcount_inc(&device->count);
    mutex_unlock(&device->lock);

    kfree(orig);
    return device;
}

/**
 * Gets an attribute from a device, optionally checking that it is the required
 * type.
 *
 * @param device        Device to get from.
 * @param name          Attribute name.
 * @param type          Required type (if -1 will not check).
 *
 * @return              Pointer to attribute structure if found, NULL if not.
 */
const device_attr_t *device_attr(device_t *device, const char *name, int type) {
    // TODO: This should really return a copy, as if we allow attributes to be
    // changed after device creation then we'd have a thread safety issue from
    // returning directly.

    assert(device);
    assert(name);

    for (size_t i = 0; i < device->attr_count; i++) {
        if (strcmp(device->attrs[i].name, name) == 0) {
            if (type != -1 && (int)device->attrs[i].type != type)
                return NULL;

            return &device->attrs[i];
        }
    }

    return NULL;
}

/** Get the path to a device.
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


/** Create a handle to a device.
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

    if (access && !file_access(&device->file, access)) {
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

/** Create a handle to a device.
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

static void dump_children(radix_tree_t *tree, int indent) {
    radix_tree_foreach(tree, iter) {
        device_t *device = radix_tree_entry(iter, device_t);

        kdb_printf(
            "%*s%-*s %-18p %-18p %d\n", indent, "",
            24 - indent, device->name, device, device->parent,
            refcount_get(&device->count));

        if (device->dest) {
            dump_children(&device->dest->children, indent + 2);
        } else {
            dump_children(&device->children, indent + 2);
        }
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
        kdb_printf("Name                     Address            Parent             Count\n");
        kdb_printf("====                     =======            ======             =====\n");

        dump_children(&device_root_dir->children, 0);
        return KDB_SUCCESS;
    }

    uint64_t val;
    if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    device_t *device = (device_t *)((ptr_t)val);

    kdb_printf("Device %p (%s)\n", device, device->name);
    kdb_printf("=================================================\n");
    kdb_printf("Count:       %d\n", refcount_get(&device->count));
    kdb_printf("Parent:      %p\n", device->parent);
    if (device->dest)
        kdb_printf("Destination: %p(%s)\n", device->dest, device->dest->name);
    kdb_printf("Ops:         %p\n", device->ops);
    kdb_printf("Data:        %p\n", device->data);

    if (!device->attrs)
        return KDB_SUCCESS;

    kdb_printf("\nAttributes:\n");

    for (size_t i = 0; i < device->attr_count; i++) {
        kdb_printf("  %s - ", device->attrs[i].name);
        switch (device->attrs[i].type) {
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

/** Initialize the device manager. */
__init_text void device_init(void) {
    status_t ret;

    /* Create the root node of the device tree. */
    device_root_dir = kcalloc(1, sizeof(device_t), MM_BOOT);

    mutex_init(&device_root_dir->lock, "device_root_lock", 0);
    refcount_set(&device_root_dir->count, 0);
    radix_tree_init(&device_root_dir->children);

    device_root_dir->name = (char *)"<root>";
    device_root_dir->time = boot_time();

    /* Create standard device directories. */
    ret = device_create("bus", device_root_dir, NULL, NULL, NULL, 0, &device_bus_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create("platform", device_bus_dir, NULL, NULL, NULL, 0, &device_bus_platform_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create("class", device_root_dir, NULL, NULL, NULL, 0, &device_class_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    ret = device_create("virtual", device_root_dir, NULL, NULL, NULL, 0, &device_virtual_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create standard device directory (%d)", ret);

    /* Register the KDB command. */
    kdb_register_command("device", "Examine the device tree.", kdb_cmd_device);
}

/** Open a handle to a device.
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
