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
 * @brief               Device Tree bus manager.
 */

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>

#include "dt.h"

/** Define to print out the device tree during boot. */
#define PRINT_DEVICE_TREE 1

static void *fdt_address;
static uint32_t fdt_size;

static dt_device_t *root_dt_device;
static AVL_TREE_DEFINE(dt_phandle_tree);

static LIST_DEFINE(builtin_dt_drivers);

/** Registers a built-in driver (see BUILTIN_DT_DRIVER). */
void __init_text dt_register_builtin_driver(dt_driver_t *driver) {
    // TODO: Register built-in drivers as bus drivers once the bus type is
    // initialised.

    list_init(&driver->builtin_link);
    list_append(&builtin_dt_drivers, &driver->builtin_link);
}

/**
 * Matches a device to a built-in driver. Marks the device as matched and sets
 * its match pointer.
 */
dt_driver_t *dt_match_builtin_driver(dt_device_t *device, builtin_dt_driver_type_t type) {
    assert(device->flags & DT_DEVICE_AVAILABLE);

    list_foreach(&builtin_dt_drivers, iter) {
        dt_driver_t *driver = list_entry(iter, dt_driver_t, builtin_link);

        if (driver->builtin_type == type) {
            for (size_t i = 0; i < driver->matches.count; i++) {
                dt_match_t *match = &driver->matches.array[i];

                if (strcmp(device->compatible, match->compatible) == 0) {
                    if (!(device->flags & DT_DEVICE_MATCHED)) {
                        device->flags |= DT_DEVICE_MATCHED;
                        device->driver = driver;
                        device->match  = match;
                        return driver;
                    } else {
                        kprintf(LOG_WARN, "dt: multiple built-in drivers match device %s\n", device->name);
                        return NULL;
                    }
                }
            }
        }
    }

    return NULL;
}

static void do_dt_iterate(dt_device_t *device, dt_iterate_cb_t cb, void *data) {
    if (device->flags & DT_DEVICE_AVAILABLE)
        cb(device, data);

    list_foreach(&device->children, iter) {
        dt_device_t *child = list_entry(iter, dt_device_t, parent_link);
        do_dt_iterate(child, cb, data);
    }
}

/** Iterate the DT device tree.
 * @param cb            Callback function call on each node.
 * @param data          Data to pass to the callback. */
void dt_iterate(dt_iterate_cb_t cb, void *data) {
    if (root_dt_device)
        do_dt_iterate(root_dt_device, cb, data);
}

/** Find a device by phandle. */
dt_device_t *dt_device_get_by_phandle(uint32_t phandle) {
    return avl_tree_lookup(&dt_phandle_tree, phandle, dt_device_t, phandle_link);
}

/** Get the FDT address. */
const void *dt_fdt_get(void) {
    return fdt_address;
}

/** Get a raw DT property.
 * @param device        Device to get for.
 * @param name          Name of property.
 * @param _value        Where to store property value pointer.
 * @param _len          Where to store property length in bytes.
 * @return              Whether property was found. */
bool dt_get_prop(dt_device_t *device, const char *name, const uint32_t **_value, uint32_t *_len) {
    int len = 0;
    const uint32_t *value = fdt_getprop(fdt_address, device->fdt_offset, name, &len);

    if (_value)
        *_value = value;
    if (_len)
        *_len = (uint32_t)len;

    return value != NULL;
}

/** Get a uint32 DT property.
 * @param device        Device to get for.
 * @param name          Name of property.
 * @param _value        Where to store property value.
 * @return              Whether property was found/valid. */
bool dt_get_prop_u32(dt_device_t *device, const char *name, uint32_t *_value) {
    int len;
    const uint32_t *prop = fdt_getprop(fdt_address, device->fdt_offset, name, &len);
    if (!prop || len != 4)
        return false;

    if (_value)
        *_value = fdt32_to_cpu(*prop);

    return true;
}

static __init_text bool is_available(int node_offset) {
    int len;
    const char *prop = fdt_getprop(fdt_address, node_offset, "status", &len);
    if (!prop)
        return true;

    if (len == 0) {
        /* Present but invalid. */
        return false;
    }

    return strcmp(prop, "ok") == 0 || strcmp(prop, "okay") == 0;
}

static __init_text dt_device_t *add_device(int node_offset, dt_device_t *parent) {
    /* Non-root devices must have a non-empty name. */
    const char *name = fdt_get_name(fdt_address, node_offset, NULL);
    if (parent && (!name || strlen(name) == 0)) {
        kprintf(LOG_WARN, "dt: cannot get name for device at offset %d, ignoring\n", node_offset);
        return NULL;
    }

    dt_device_t *device = kmalloc(sizeof(*device), MM_BOOT);

    list_init(&device->parent_link);
    list_init(&device->children);

    if (parent)
        list_append(&parent->children, &device->parent_link);

    device->fdt_offset = node_offset;
    device->phandle    = fdt_get_phandle(fdt_address, node_offset);
    device->name       = (parent) ? name : "/";
    device->compatible = fdt_getprop(fdt_address, node_offset, "compatible", NULL);
    device->parent     = parent;
    device->flags      = (is_available(node_offset)) ? DT_DEVICE_AVAILABLE : 0;

    if (device->phandle != 0)
        avl_tree_insert(&dt_phandle_tree, device->phandle, &device->phandle_link);

    for (int child_offset = fdt_first_subnode(fdt_address, node_offset);
         child_offset >= 0;
         child_offset = fdt_next_subnode(fdt_address, child_offset))
    {
        add_device(child_offset, device);
    }

    return device;
}

#ifdef PRINT_DEVICE_TREE

static __init_text void print_device(dt_device_t *device, int depth) {
    kprintf(
        LOG_DEBUG, "%*s%s (compatible: '%s', available: %s)\n",
        (depth + 1) * 2, "", device->name,
        (device->compatible) ? device->compatible : "<none>",
        (device->flags & DT_DEVICE_AVAILABLE) ? "yes" : "no");

    list_foreach(&device->children, iter) {
        dt_device_t *child = list_entry(iter, dt_device_t, parent_link);
        print_device(child, depth + 1);
    }
}

#endif

/**
 * Early initialisation of DT. Sets up enough for low-level devices (IRQ
 * controllers, timers, etc.) to function.
 */
static __init_text void dt_early_init(void) {
    kboot_tag_fdt_t *tag = kboot_tag_iterate(KBOOT_TAG_FDT, NULL);
    if (!tag)
        fatal("Boot loader did not supply FDT");

    /* Make our own copy of the FDT since KBoot puts it in reclaimable memory. */
    fdt_size    = tag->size;
    fdt_address = kmalloc(fdt_size, MM_BOOT);
    memcpy(fdt_address, (void *)((ptr_t)tag->addr_virt), fdt_size);

    int ret = fdt_check_header(fdt_address);
    if (ret != 0)
        fatal("FDT header validation failed (%d)", ret);

    /* Create structures for all devices. */
    root_dt_device = add_device(0, NULL);

    if (!root_dt_device) {
        kprintf(LOG_WARN, "dt: no devices found in the FDT\n");
    } else {
        #ifdef PRINT_DEVICE_TREE
            kprintf(LOG_DEBUG, "dt: found devices:\n");
            print_device(root_dt_device, 0);
        #endif
    }
}

INITCALL_TYPE(dt_early_init, INITCALL_TYPE_EARLY_DEVICE);
