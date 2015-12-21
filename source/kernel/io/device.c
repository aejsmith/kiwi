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
device_t *device_tree_root;

/** Standard device directories. */
device_t *device_bus_dir;

/** Close a device.
 * @param handle        File handle structure. */
static void device_file_close(file_handle_t *handle) {
    if (handle->device->ops && handle->device->ops->close)
        handle->device->ops->close(handle->device, handle);

    refcount_dec(&handle->device->count);
}

/** Signal that a device event is being waited for.
 * @param handle        File handle structure.
 * @param event         Event that is being waited for.
 * @return              Status code describing result of the operation. */
static status_t device_file_wait(file_handle_t *handle, object_event_t *event) {
    device_t *device = handle->device;

    return (device->ops && device->ops->wait && device->ops->unwait)
        ? device->ops->wait(device, handle, event)
        : STATUS_INVALID_EVENT;
}

/** Stop waiting for a device event.
 * @param handle        File handle structure.
 * @param event         Event that is being waited for. */
static void device_file_unwait(file_handle_t *handle, object_event_t *event) {
    assert(handle->device->ops);
    return handle->device->ops->unwait(handle->device, handle, event);
}

/** Perform I/O on a device.
 * @param handle        File handle structure.
 * @param request       I/O request.
 * @return              Status code describing result of the operation. */
static status_t device_file_io(file_handle_t *handle, io_request_t *request) {
    return (handle->device->ops && handle->device->ops->io)
        ? handle->device->ops->io(handle->device, handle, request)
        : STATUS_NOT_SUPPORTED;
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

/** Get information about a device.
 * @param handle        File handle structure.
 * @param info          Information structure to fill in. */
static void device_file_info(file_handle_t *handle, file_info_t *info) {
    info->id = 0;
    info->mount = 0;
    info->type = handle->file->type;
    info->block_size = 0;
    info->size = 0;
    info->links = 1;
    info->created = info->accessed = info->modified = boot_time();
}

/** Device file operations structure. */
static file_ops_t device_file_ops = {
    .close = device_file_close,
    .wait = device_file_wait,
    .unwait = device_file_unwait,
    .io = device_file_io,
    .map = device_file_map,
    .info = device_file_info,
};

/**
 * Create a new device tree node.
 *
 * Creates a new node in the device tree. The device created will not have a
 * reference on it. The device can have no operations, in which case it will
 * simply act as a container for other devices.
 *
 * @param name          Name of device to create (will be duplicated).
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
status_t device_create(
    const char *name, device_t *parent, device_ops_t *ops, void *data,
    device_attr_t *attrs, size_t count, device_t **_device)
{
    device_t *device = NULL;
    status_t ret;
    size_t i;

    assert(name);
    assert(strlen(name) < DEVICE_NAME_MAX);
    assert(parent);
    assert(!parent->dest);

    if (ops)
        assert(ops->type == FILE_TYPE_BLOCK || ops->type == FILE_TYPE_CHAR);

    /* Check if a child already exists with this name. */
    mutex_lock(&parent->lock);
    if (radix_tree_lookup(&parent->children, name)) {
        ret = STATUS_ALREADY_EXISTS;
        goto fail;
    }

    device = kmalloc(sizeof(device_t), MM_KERNEL);
    device->file.ops = &device_file_ops;
    device->file.type = (ops) ? ops->type : FILE_TYPE_CHAR;
    mutex_init(&device->lock, "device_lock", 0);
    refcount_set(&device->count, 0);
    radix_tree_init(&device->children);
    list_init(&device->aliases);
    device->name = kstrdup(name, MM_KERNEL);
    device->parent = parent;
    device->dest = NULL;
    device->ops = ops;
    device->data = data;
    if (attrs) {
        /* Ensure the attribute structures are valid. Do validity checking
         * before allocating anything to make it easier to clean up if an
         * invalid structure is found. */
        for (i = 0; i < count; i++) {
            if (!attrs[i].name || strlen(attrs[i].name) >= DEVICE_NAME_MAX) {
                ret = STATUS_INVALID_ARG;
                goto fail;
            } else if (attrs[i].type == DEVICE_ATTR_STRING) {
                if (!attrs[i].value.string || strlen(attrs[i].value.string) >= DEVICE_ATTR_MAX) {
                    ret = STATUS_INVALID_ARG;
                    goto fail;
                }
            }
        }

        /* Duplicate the structures, then fix up the data. */
        device->attrs = kmemdup(attrs, sizeof(device_attr_t) * count, MM_KERNEL);
        device->attr_count = count;
        for (i = 0; i < device->attr_count; i++) {
            device->attrs[i].name = kstrdup(device->attrs[i].name, MM_KERNEL);
            if (device->attrs[i].type == DEVICE_ATTR_STRING)
                device->attrs[i].value.string = kstrdup(device->attrs[i].value.string, MM_KERNEL);
        }
    } else {
        device->attrs = NULL;
        device->attr_count = 0;
    }

    /* Attach to the parent. */
    refcount_inc(&parent->count);
    radix_tree_insert(&parent->children, device->name, device);
    mutex_unlock(&parent->lock);

    kprintf(
        LOG_DEBUG, "device: created device %s in %s (ops: %p, data: %p)\n",
        device->name, parent->name, ops, data);

    if (_device)
        *_device = device;

    return STATUS_SUCCESS;

fail:
    if (device) {
        kfree(device->name);
        kfree(device);
    }

    mutex_unlock(&parent->lock);
    return ret;
}

/**
 * Create an alias for a device.
 *
 * Creates an alias for another device in the device tree. Any attempts to open
 * the alias will open the device it is an alias for.
 *
 * @param name          Name to give alias.
 * @param parent        Device to create alias under.
 * @param dest          Destination device. If this is an alias, the new alias
 *                      will refer to the destination, not the alias itself.
 * @param _device       Where to store pointer to alias structure (can be NULL).
 *
 * @return              Status code describing result of the operation.
 */
status_t device_alias(const char *name, device_t *parent, device_t *dest, device_t **_device) {
    device_t *device;

    assert(name);
    assert(strlen(name) < DEVICE_NAME_MAX);
    assert(parent);
    assert(!parent->dest);
    assert(dest);

    /* If the destination is an alias, use it's destination. */
    if (dest->dest)
        dest = dest->dest;

    /* Check if a child already exists with this name. */
    mutex_lock(&parent->lock);
    if (radix_tree_lookup(&parent->children, name)) {
        mutex_unlock(&parent->lock);
        return STATUS_ALREADY_EXISTS;
    }

    device = kmalloc(sizeof(device_t), MM_KERNEL);
    device->file.ops = &device_file_ops;
    device->file.type = (dest->ops) ? dest->ops->type : FILE_TYPE_CHAR;
    mutex_init(&device->lock, "device_alias_lock", 0);
    refcount_set(&device->count, 0);
    radix_tree_init(&device->children);
    list_init(&device->dest_link);
    device->name = kstrdup(name, MM_KERNEL);
    device->parent = parent;
    device->dest = dest;
    device->ops = NULL;
    device->data = NULL;
    device->attrs = NULL;
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
 * Remove a device from the device tree.
 *
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
    device_t *alias;
    size_t i;

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
            alias = list_entry(iter, device_t, dest_link);
            device_destroy(alias);
        }
    }

    radix_tree_remove(&device->parent->children, device->name, NULL);
    refcount_dec(&device->parent->count);
    mutex_unlock(&device->parent->lock);

    /* Free up attributes if any. */
    if (device->attrs) {
        for (i = 0; i < device->attr_count; i++) {
            kfree((char *)device->attrs[i].name);
            if (device->attrs[i].type == DEVICE_ATTR_STRING)
                kfree((char *)device->attrs[i].value.string);
        }

        kfree(device->attrs);
    }

    kprintf(LOG_DEBUG, "device: destroyed device %s\n", device->name);

    kfree(device->name);
    kfree(device);
    return STATUS_SUCCESS;
}

/** Internal iteration function.
 * @param device        Device we're currently on.
 * @param func          Function to call on devices.
 * @param data          Data argument to pass to function.
 * @return              Whether to continue lookup. */
static bool device_iterate_internal(device_t *device, device_iterate_t func, void *data) {
    device_t *child;

    switch (func(device, data)) {
    case DEVICE_ITERATE_END:
        return false;
    case DEVICE_ITERATE_DESCEND:
        radix_tree_foreach(&device->children, iter) {
            child = radix_tree_entry(iter, device_t);

            if (!device_iterate_internal(child, func, data))
                return false;
        }
    case DEVICE_ITERATE_RETURN:
        return true;
    }

    return false;
}

/**
 * Iterate through the device tree.
 *
 * Calls the specified function on a device and all its children (and all their
 * children, etc).
 *
 * @todo                We have small kernel stacks. Recursive lookup probably
 *                      isn't a very good idea. Then again, the device tree
 *                      shouldn't go *too* deep.
 *
 * @param start         Starting device.
 * @param func          Function to call on devices.
 * @param data          Data argument to pass to function.
 */
void device_iterate(device_t *start, device_iterate_t func, void *data) {
    device_iterate_internal(start, func, data);
}

/** Look up a device and increase its reference count.
 * @param path          Path to device.
 * @return              Pointer to device if found, NULL if not. */
static device_t *device_lookup(const char *path) {
    device_t *device = device_tree_root, *child;
    char *dup, *orig, *tok;

    assert(path);

    if (!path[0] || path[0] != '/')
        return NULL;

    dup = orig = kstrdup(path, MM_KERNEL);

    mutex_lock(&device->lock);

    while ((tok = strsep(&dup, "/"))) {
        if (!tok[0])
            continue;

        child = radix_tree_lookup(&device->children, tok);
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
    return device;
}

/**
 * Get a device attribute.
 *
 * Gets an attribute from a device, and optionally checks that it is the
 * required type. Returned structure must NOT be modified.
 *
 * @param device        Device to get from.
 * @param name          Attribute name.
 * @param type          Required type (if -1 will not check).
 *
 * @return              Pointer to attribute structure if found, NULL if not.
 */
device_attr_t *device_attr(device_t *device, const char *name, int type) {
    size_t i;

    assert(device);
    assert(name);

    for (i = 0; i < device->attr_count; i++) {
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
    char *path = NULL, *tmp;
    device_t *parent;
    size_t len = 0;

    while (device != device_tree_root) {
        mutex_lock(&device->lock);
        len += strlen(device->name) + 1;
        tmp = kmalloc(len + 1, MM_KERNEL);
        strcpy(tmp, "/");
        strcat(tmp, device->name);
        if (path) {
            strcat(tmp, path);
            kfree(path);
        }
        path = tmp;
        parent = device->parent;
        mutex_unlock(&device->lock);
        device = parent;
    }

    if (!len)
        path = kstrdup("/", MM_KERNEL);

    return path;
}


/** Get a handle to a device.
 * @param device        Device to get handle to.
 * @param rights        Requested rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store handle to device.
 * @return              Status code describing result of the operation. */
status_t device_get(device_t *device, uint32_t rights, uint32_t flags, object_handle_t **_handle) {
    file_handle_t *handle;
    status_t ret;

    assert(device);
    assert(_handle);

    mutex_lock(&device->lock);

    if (rights && !file_access(&device->file, rights)) {
        mutex_unlock(&device->lock);
        return STATUS_ACCESS_DENIED;
    }

    handle = file_handle_alloc(&device->file, rights, flags);

    if (device->ops && device->ops->open) {
        ret = device->ops->open(device, flags, &handle->private);
        if (ret != STATUS_SUCCESS) {
            file_handle_free(handle);
            mutex_unlock(&device->lock);
            return ret;
        }
    }

    refcount_inc(&device->count);
    *_handle = file_handle_create(handle);
    mutex_unlock(&device->lock);
    return STATUS_SUCCESS;
}

/** Create a handle to a device.
 * @param path          Path to device to open.
 * @param rights        Requested rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store pointer to handle structure.
 * @return              Status code describing result of the operation. */
status_t device_open(const char *path, uint32_t rights, uint32_t flags, object_handle_t **_handle) {
    device_t *device;
    file_handle_t *handle;
    status_t ret;

    assert(path);
    assert(_handle);

    device = device_lookup(path);
    if (!device)
        return STATUS_NOT_FOUND;

    mutex_lock(&device->lock);

    if (rights && !file_access(&device->file, rights)) {
        refcount_dec(&device->count);
        mutex_unlock(&device->lock);
        return STATUS_ACCESS_DENIED;
    }

    handle = file_handle_alloc(&device->file, rights, flags);

    if (device->ops && device->ops->open) {
        ret = device->ops->open(device, flags, &handle->private);
        if (ret != STATUS_SUCCESS) {
            file_handle_free(handle);
            refcount_dec(&device->count);
            mutex_unlock(&device->lock);
            return ret;
        }
    }

    *_handle = file_handle_create(handle);
    mutex_unlock(&device->lock);
    return STATUS_SUCCESS;
}

/** Perform a device-specific operation.
 * @param handle        Handle to device to perform operation on.
 * @param request       Operation number to perform.
 * @param in            Optional input buffer containing data to pass to the
 *                      operation handler.
 * @param in_size       Size of input buffer.
 * @param _out          Where to store pointer to data returned by the
 *                      operation handler (optional).
 * @param _out_size     Where to store size of data returned.
 * @return              Status code describing result of the operation. */
status_t device_request(
    object_handle_t *handle, unsigned request, const void *in, size_t in_size,
    void **_out, size_t *_out_size)
{
    file_handle_t *data;
    device_t *device;

    assert(handle);

    if (handle->type->id != OBJECT_TYPE_FILE)
        return STATUS_INVALID_HANDLE;

    data = (file_handle_t *)handle->private;
    device = (device_t *)data->file;

    if (device->file.ops != &device_file_ops) {
        return STATUS_INVALID_HANDLE;
    } else if (!device->ops || !device->ops->request) {
        return STATUS_INVALID_REQUEST;
    }

    return device->ops->request(device, data, request, in, in_size, _out, _out_size);
}

/** Print out a device's children.
 * @param tree          Radix tree to print.
 * @param indent        Indentation level. */
static void dump_children(radix_tree_t *tree, int indent) {
    device_t *device;

    radix_tree_foreach(tree, iter) {
        device = radix_tree_entry(iter, device_t);

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

/** Print out device information.
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @return              KDB status code. */
static kdb_status_t kdb_cmd_device(int argc, char **argv, kdb_filter_t *filter) {
    device_t *device;
    uint64_t val;
    size_t i;

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

        dump_children(&device_tree_root->children, 0);
        return KDB_SUCCESS;
    }

    if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    device = (device_t *)((ptr_t)val);

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

    for (i = 0; i < device->attr_count; i++) {
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
    device_tree_root = kcalloc(1, sizeof(device_t), MM_BOOT);
    mutex_init(&device_tree_root->lock, "device_root_lock", 0);
    refcount_set(&device_tree_root->count, 0);
    radix_tree_init(&device_tree_root->children);
    device_tree_root->name = (char *)"<root>";

    /* Create standard device directories. */
    ret = device_create("bus", device_tree_root, NULL, NULL, NULL, 0, &device_bus_dir);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create bus directory in device tree (%d)", ret);

    /* Register the KDB command. */
    kdb_register_command("device", "Examine the device tree.", kdb_cmd_device);
}

/** Open a handle to a device.
 * @param path          Device tree path for device to open.
 * @param rights        Requested rights for the handle.
 * @param flags         Behaviour flags for the handle.
 * @param _handle       Where to store handle to the device.
 * @return              Status code describing result of the operation. */
status_t kern_device_open(const char *path, uint32_t rights, uint32_t flags, handle_t *_handle) {
    object_handle_t *handle;
    status_t ret;
    char *kpath;

    if (!path || !_handle)
        return STATUS_INVALID_ARG;

    ret = strndup_from_user(path, DEVICE_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = device_open(kpath, rights, flags, &handle);
    if (ret != STATUS_SUCCESS) {
        kfree(kpath);
        return ret;
    }

    ret = object_handle_attach(handle, NULL, _handle);
    object_handle_release(handle);
    kfree(kpath);
    return ret;
}

/** Perform a device-specific operation.
 * @param handle        Handle to device to perform operation on.
 * @param request       Operation number to perform.
 * @param in            Optional input buffer containing data to pass to the
 *                      operation handler.
 * @param in_size       Size of input buffer.
 * @param out           Optional output buffer.
 * @param out_size      Size of output buffer.
 * @param _bytes        Where to store number of bytes copied into output buffer.
 * @return              Status code describing result of the operation. */
status_t kern_device_request(
    handle_t handle, unsigned request, const void *in, size_t in_size,
    void *out, size_t out_size, size_t *_bytes)
{
    void *kin = NULL, *kout = NULL;
    object_handle_t *khandle;
    status_t ret, err;
    size_t koutsz;

    if (in_size && !in)
        return STATUS_INVALID_ARG;
    if (out_size && !out)
        return STATUS_INVALID_ARG;

    ret = object_handle_lookup(handle, OBJECT_TYPE_FILE, &khandle);
    if (ret != STATUS_SUCCESS)
        goto out;

    if (in_size) {
        kin = kmalloc(in_size, MM_USER);
        if (!kin) {
            ret = STATUS_NO_MEMORY;
            goto out;
        }

        ret = memcpy_from_user(kin, in, in_size);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    ret = device_request(khandle, request, kin, in_size, (out) ? &kout : NULL, (out) ? &koutsz : NULL);

    if (kout) {
        if (koutsz > out_size) {
            ret = STATUS_TOO_SMALL;
            goto out;
        }

        err = memcpy_to_user(out, kout, koutsz);
        if (err != STATUS_SUCCESS) {
            ret = err;
            goto out;
        }

        if (_bytes) {
            err = write_user(_bytes, koutsz);
            if (err != STATUS_SUCCESS)
                ret = err;
        }
    }

out:
    if (kin)
        kfree(kin);
    if (kout)
        kfree(kout);

    object_handle_release(khandle);
    return ret;
}
