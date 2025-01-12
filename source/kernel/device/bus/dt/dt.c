/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device Tree bus manager.
 */

#include <device/device.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

#include "dt.h"

/** Define to print out the device tree during boot. */
#define PRINT_DEVICE_TREE 1

static void *fdt_address;
static uint32_t fdt_size;

static dt_device_t *root_dt_device;
static AVL_TREE_DEFINE(dt_phandle_tree);

static LIST_DEFINE(builtin_dt_drivers);

bus_t dt_bus;

/**
 * Sets the driver that a device is matched to, and sets up things such as
 * IRQs before initialising the driver.
 */
bool dt_device_match(dt_device_t *device, dt_driver_t *driver, dt_match_t *match) {
    assert(!(device->flags & DT_DEVICE_MATCHED));

    device->flags |= DT_DEVICE_MATCHED;
    device->driver = driver;
    device->match  = match;

    if (!dt_irq_init_device(device)) {
        dt_device_unmatch(device);
        return false;
    }

    return true;
}

/** Unmatches a device from its current driver. */
void dt_device_unmatch(dt_device_t *device) {
    dt_irq_deinit_device(device);

    device->driver = NULL;
    device->match  = NULL;
    device->flags &= ~DT_DEVICE_MATCHED;
}

/** Registers a built-in driver (see BUILTIN_DT_DRIVER). */
void __init_text dt_register_builtin_driver(dt_driver_t *driver) {
    // TODO: Register built-in drivers as bus drivers once the bus type is
    // initialised.

    list_init(&driver->builtin_link);
    list_append(&builtin_dt_drivers, &driver->builtin_link);
}

/** Gets the symbol name for a builtin driver. */
const char *dt_get_builtin_driver_name(dt_driver_t *driver) {
    symbol_t sym;
    symbol_from_addr((ptr_t)driver, &sym, NULL);
    return sym.name;
}

/**
 * Matches a device to a built-in driver. Marks the device as matched and sets
 * its match pointer.
 */
dt_driver_t *dt_match_builtin_driver(dt_device_t *device, builtin_dt_driver_type_t type) {
    assert(device->flags & DT_DEVICE_AVAILABLE);

    /*
     * For multiple compatible strings, they are ordered most to least specific
     * so we want to try matching in that order to get the best match.
     */
    for (size_t compatible_idx = 0; compatible_idx < device->compatible.count; compatible_idx++) {
        const char **compatible = array_entry(&device->compatible, const char *, compatible_idx);

        list_foreach(&builtin_dt_drivers, iter) {
            dt_driver_t *driver = list_entry(iter, dt_driver_t, builtin_link);

            if (driver->builtin_type == type) {
                for (size_t match_idx = 0; match_idx < driver->matches.count; match_idx++) {
                    dt_match_t *match = &driver->matches.array[match_idx];

                    if (strcmp(*compatible, match->compatible) == 0) {
                        kprintf(
                            LOG_DEBUG, "dt: matched device %s to built-in driver %s\n",
                            device->name, dt_get_builtin_driver_name(driver));

                        if (!(device->flags & DT_DEVICE_MATCHED)) {
                            if (dt_device_match(device, driver, match)) {
                                return driver;
                            } else {
                                return NULL;
                            }
                        } else {
                            kprintf(LOG_WARN, "dt: multiple built-in drivers match device %s\n", device->name);
                            return NULL;
                        }
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

/** Iterates the DT device tree.
 * @param cb            Callback function call on each node.
 * @param data          Data to pass to the callback. */
void dt_iterate(dt_iterate_cb_t cb, void *data) {
    if (root_dt_device)
        do_dt_iterate(root_dt_device, cb, data);
}

/** Finds a device by phandle. */
dt_device_t *dt_device_get_by_phandle(uint32_t phandle) {
    return avl_tree_lookup(&dt_phandle_tree, phandle, dt_device_t, phandle_link);
}

/** Gets a value from a property.
 * @param ptr           Pointer to value.
 * @param num_cells     Number of cells per value.
 * @return              Value read. */
uint64_t dt_get_value(const uint32_t *ptr, uint32_t num_cells) {
    assert(num_cells == 1 || num_cells == 2);

    uint64_t value = 0;

    for (uint32_t i = 0; i < num_cells; i++) {
        value = value << 32;
        value |= fdt32_to_cpu(*(ptr));
        ptr++;
    }

    return value;
}

/** Gets a raw DT property.
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

/** Gets a uint32 DT property.
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

/** Gets the FDT address. */
const void *dt_fdt_get(void) {
    return fdt_address;
}

static uint32_t get_num_cells(dt_device_t *device, const char *name, uint32_t def) {
    while (device) {
        const uint32_t *prop;
        uint32_t len;
        if (dt_get_prop(device, name, &prop, &len))
            return fdt32_to_cpu(*prop);

        device = device->parent;
    }

    return def;
}

/** Gets the number of address cells for a device. */
uint32_t dt_get_address_cells(dt_device_t *device) {
    return get_num_cells(device, "#address-cells", 2);
}

/** Gets the number of size cells for a device. */
uint32_t dt_get_size_cells(dt_device_t *device) {
    return get_num_cells(device, "#size-cells", 1);
}

static phys_ptr_t translate_address(dt_device_t *device, phys_ptr_t address) {
    uint32_t parent_address_cells = 0;
    uint32_t parent_size_cells = 0;
    bool first = true;

    while (device) {
        uint32_t node_address_cells = parent_address_cells;
        uint32_t node_size_cells    = parent_size_cells;

        if (device->parent) {
            parent_address_cells = dt_get_address_cells(device->parent);
            parent_size_cells    = dt_get_size_cells(device->parent);
        } else {
            parent_address_cells = 2;
            parent_size_cells    = 1;
        }

        if (first) {
            /* Just need cells details from the parent to start, but we start
             * searching for ranges from the parent. */
            first = false;
        } else {
            const uint32_t *prop;
            uint32_t len;
            if (dt_get_prop(device, "ranges", &prop, &len)) {
                /* Each entry is a (child-address, parent-address, child-length) triplet. */
                uint32_t entry_cells = node_address_cells + parent_address_cells + node_size_cells;
                uint32_t entries     = dt_get_num_entries(len, entry_cells);

                for (uint32_t i = 0; i < entries; i++) {
                    uint64_t node_base   = dt_get_value(prop, node_address_cells);
                    prop += node_address_cells;
                    uint64_t parent_base = dt_get_value(prop, parent_address_cells);
                    prop += parent_address_cells;
                    uint64_t length      = dt_get_value(prop, node_size_cells);
                    prop += node_size_cells;

                    /* Translate if within the range. */
                    if (address >= node_base && address < (node_base + length)) {
                        address = (address - node_base) + parent_base;
                        break;
                    }
                }
            }
        }

        device = device->parent;
    }

    return address;
}

/** Gets a register address for a device.
 * @param device        Device to get for.
 * @param index         Register index.
 * @param _address      Where to store register adddress.
 * @param _size         Where to store register size.
 * @return              Whether found. */
bool dt_reg_get(dt_device_t *device, uint8_t index, phys_ptr_t *_address, phys_size_t *_size) {
    uint32_t address_cells = dt_get_address_cells(device);
    uint32_t size_cells    = dt_get_size_cells(device);
    uint32_t total_cells   = address_cells + size_cells;

    const uint32_t *prop;
    uint32_t len;
    if (!dt_get_prop(device, "reg", &prop, &len))
        return false;

    uint32_t entries = dt_get_num_entries(len, total_cells);
    if (index >= entries)
        return false;

    phys_ptr_t address = dt_get_value(&prop[index * total_cells], address_cells);
    phys_size_t size   = dt_get_value(&prop[(index * total_cells) + address_cells], size_cells);

    *_address = translate_address(device, address);
    *_size    = size;
    return true;
}

/**
 * Maps a DT device register. This will return an I/O region handle to the
 * register.
 *
 * The mapping will be created with MMU_ACCESS_RW and MMU_CACHE_DEVICE. Use
 * dt_reg_map_etc() to change this.
 *
 * The full detected range of the register is mapped. To map only a sub-range,
 * use dt_reg_map_etc().
 *
 * The region should be unmapped with dt_reg_unmap(), which will handle passing
 * the correct size to io_unmap().
 *
 * @param device        Device to map for.
 * @param index         Index of the register to map.
 * @param mmflag        Allocation behaviour flags.
 * @param _region       Where to store I/O region handle.
 *
 * @return              STATUS_NOT_FOUND if register does not exist.
 *                      STATUS_NO_MEMORY if mapping the register failed.
 */
status_t dt_reg_map(dt_device_t *device, uint8_t index, uint32_t mmflag, io_region_t *_region) {
    return dt_reg_map_etc(device, index, 0, 0, MMU_ACCESS_RW | MMU_CACHE_DEVICE, mmflag, _region);
}

/**
 * Maps a DT device register. This will return an I/O region handle to the
 * register.
 *
 * The mapping will be created with the specified access and cache mode.
 *
 * This allows only a sub-range of the register to be mapped. An error will be
 * returned if the specified range goes outside of the maximum register range.
 *
 * The region should be unmapped with dt_reg_unmap_etc(), which will handle
 * passing the correct size to io_unmap(). dt_reg_unmap() can be used if the
 * offset and size are both 0.
 *
 * @param device        Device to map for.
 * @param index         Index of the register to map.
 * @param offset        Offset into the register to map from.
 * @param size          Size of the range to map (if 0, will map the whole
 *                      register from the offset).
 * @param flags         MMU mapping flags.
 * @param mmflag        Allocation behaviour flags.
 * @param _region       Where to store I/O region handle.
 *
 * @return              STATUS_NOT_FOUND if register does not exist.
 *                      STATUS_INVALID_ADDR if range is outside of the register.
 *                      STATUS_NO_MEMORY if mapping the register failed.
 */
status_t dt_reg_map_etc(
    dt_device_t *device, uint8_t index, phys_ptr_t offset, phys_size_t size,
    uint32_t flags, uint32_t mmflag, io_region_t *_region)
{
    phys_ptr_t reg_base;
    phys_size_t reg_size;
    if (!dt_reg_get(device, index, &reg_base, &reg_size))
        return STATUS_NOT_FOUND;

    /* Validate offset and size. */
    if (offset >= reg_size)
        return STATUS_INVALID_ADDR;

    phys_ptr_t map_base = reg_base + offset;
    phys_ptr_t map_size = (size == 0) ? reg_size - offset : size;

    if (offset + map_size > reg_size)
        return STATUS_INVALID_ADDR;

    io_region_t region = mmio_map_etc(map_base, map_size, flags, mmflag);
    if (region == IO_REGION_INVALID)
        return STATUS_NO_MEMORY;

    *_region = region;
    return STATUS_SUCCESS;
}

/** Unmaps a previously mapped register from dt_reg_map().
 * @param device        Device to unmap for.
 * @param index         Index of the register to unmap.
 * @param region        I/O region handle to unmap. */
void dt_reg_unmap(dt_device_t *device, uint8_t index, io_region_t region) {
    dt_reg_unmap_etc(device, index, region, 0, 0);
}

/** Unmaps a previously mapped register sub-range from dt_reg_map_etc().
 * @param device        Device to unmap for.
 * @param index         Index of the register to unmap.
 * @param region        I/O region handle to unmap.
 * @param offset        Offset into the register that was mapped.
 * @param size          Size of the range that was mapped. */
void dt_reg_unmap_etc(
    dt_device_t *device, uint8_t index, io_region_t region, phys_ptr_t offset,
    phys_size_t size)
{
    phys_ptr_t reg_base;
    phys_size_t reg_size;
    bool found __unused = dt_reg_get(device, index, &reg_base, &reg_size);
    assert(found);

    phys_ptr_t map_size = (size == 0) ? reg_size - offset : size;

    assert(offset + map_size <= reg_size);

    io_unmap(region, map_size);
}

/**
 * Maps a DT device register, as a device-managed resource (will be unmapped
 * when the device is destroyed).
 *
 * @see                 dt_reg_map().
 *
 * @param owner         Device to register to.
 */
status_t device_dt_reg_map(
    device_t *owner, dt_device_t *device, uint8_t index, uint32_t mmflag,
    io_region_t *_region)
{
    return device_dt_reg_map_etc(owner, device, index, 0, 0, MMU_ACCESS_RW | MMU_CACHE_DEVICE, mmflag, _region);
}

/**
 * Maps a DT device register, as a device-managed resource (will be unmapped
 * when the device is destroyed).
 *
 * @see                 dt_reg_map_etc().
 *
 * @param owner         Device to register to.
 */
status_t device_dt_reg_map_etc(
    device_t *owner, dt_device_t *device, uint8_t index, phys_ptr_t offset,
    phys_size_t size, uint32_t flags, uint32_t mmflag, io_region_t *_region)
{
    phys_ptr_t reg_base;
    phys_size_t reg_size;
    if (!dt_reg_get(device, index, &reg_base, &reg_size))
        return STATUS_NOT_FOUND;

    /* Validate offset and size. */
    if (offset >= reg_size)
        return STATUS_INVALID_ADDR;

    phys_ptr_t map_base = reg_base + offset;
    phys_ptr_t map_size = (size == 0) ? reg_size - offset : size;

    if (offset + map_size > reg_size)
        return STATUS_INVALID_ADDR;

    io_region_t region = device_mmio_map_etc(owner, map_base, map_size, flags, mmflag);
    if (region == IO_REGION_INVALID)
        return STATUS_NO_MEMORY;

    *_region = region;
    return STATUS_SUCCESS;
}

static bool dt_bus_match_device(bus_device_t *_device, bus_driver_t *_driver) {
    dt_device_t *device = cast_dt_device(_device);
    dt_driver_t *driver = cast_dt_driver(_driver);

    /*
     * For multiple compatible strings, they are ordered most to least specific
     * so we want to try matching in that order to get the best match.
     */
    for (size_t compatible_idx = 0; compatible_idx < device->compatible.count; compatible_idx++) {
        const char **compatible = array_entry(&device->compatible, const char *, compatible_idx);

        for (size_t match_idx = 0; match_idx < driver->matches.count; match_idx++) {
            dt_match_t *match = &driver->matches.array[match_idx];

            if (strcmp(*compatible, match->compatible) == 0) {
                if (!(device->flags & DT_DEVICE_MATCHED)) {
                    return dt_device_match(device, driver, match);
                } else {
                    kprintf(LOG_WARN, "dt: multiple drivers match device %s\n", device->name);
                    return false;
                }
            }
        }
    }

    return false;
}

static status_t dt_bus_init_device(bus_device_t *_device, bus_driver_t *_driver) {
    dt_device_t *device = cast_dt_device(_device);
    dt_driver_t *driver = cast_dt_driver(_driver);

    return (driver->init_device)
        ? driver->init_device(device)
        : STATUS_SUCCESS;
}

static bus_type_t dt_bus_type = {
    .name         = "dt",
    .device_class = DT_DEVICE_CLASS_NAME,
    .match_device = dt_bus_match_device,
    .init_device  = dt_bus_init_device,
};

static __init_text void add_bus_device(dt_device_t *device) {
    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = DT_DEVICE_CLASS_NAME } },
    };

    const char *name = (device == root_dt_device) ? "root" : device->name;
    device_t *parent = (device == root_dt_device) ? dt_bus.dir : device->parent->bus.node;

    bus_device_init(&device->bus);

    status_t ret = device_create(name, parent, NULL, NULL, attrs, array_size(attrs), &device->bus.node);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create DT device %s (%d)", name, ret);

    device_publish(device->bus.node);
    bus_match_device(&dt_bus, &device->bus);

    list_foreach(&device->children, iter) {
        dt_device_t *child = list_entry(iter, dt_device_t, parent_link);

        add_bus_device(child);
    }
}

/** Full initialisation of DT, registers the bus device. */
static __init_text void dt_bus_init(void) {
    status_t ret = bus_init(&dt_bus, &dt_bus_type);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to register DT bus (%d)", ret);

    add_bus_device(root_dt_device);
}

INITCALL_TYPE(dt_bus_init, INITCALL_TYPE_DEVICE);

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

    dt_device_t *device = kmalloc(sizeof(*device), MM_BOOT | MM_ZERO);

    array_init(&device->compatible);
    list_init(&device->parent_link);
    list_init(&device->children);

    device->fdt_offset = node_offset;
    device->phandle    = fdt_get_phandle(fdt_address, node_offset);
    device->name       = (parent) ? name : "/";
    device->parent     = parent;
    device->flags      = (is_available(node_offset)) ? DT_DEVICE_AVAILABLE : 0;

    int compat_len;
    const char *compat_str = fdt_getprop(fdt_address, node_offset, "compatible", &compat_len);
    if (compat_str) {
        const char *curr = compat_str;
        int pos = 0;
        while (pos < compat_len) {
            if (compat_str[pos] == 0) {
                const char **str = array_append(&device->compatible, const char *);
                *str = curr;

                curr = &compat_str[pos + 1];
            }

            pos++;
        }
    }

    if (parent)
        list_append(&parent->children, &device->parent_link);

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
        LOG_DEBUG, "%*s%s (available: %s, compatible: ",
        (depth + 1) * 2, "", device->name,
        (device->flags & DT_DEVICE_AVAILABLE) ? "yes" : "no");

    for (size_t i = 0; i < device->compatible.count; i++) {
        const char **str = array_entry(&device->compatible, const char *, i);
        kprintf(LOG_DEBUG, "%s'%s'", (i != 0) ? ", " : "", *str);
    }

    kprintf(LOG_DEBUG, ")\n");

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

static __init_text void init_builtin_device(dt_device_t *device, void *_type) {
    builtin_dt_driver_type_t type = *(builtin_dt_driver_type_t *)_type;

    if (dt_match_builtin_driver(device, type)) {
        status_t ret = device->driver->init_builtin(device);
        if (ret != STATUS_SUCCESS) {
            kprintf(
                LOG_ERROR, "dt: failed to initialise device %s with built-in driver %s: %d\n",
                device->name, dt_get_builtin_driver_name(device->driver), ret);
        }
    }
}

static __init_text void init_builtin_devices(builtin_dt_driver_type_t type) {
    dt_iterate(init_builtin_device, &type);
}

/** Init time devices from DT. */
static __init_text void dt_time_init(void) {
    init_builtin_devices(BUILTIN_DT_DRIVER_TIME);
}

INITCALL_TYPE(dt_time_init, INITCALL_TYPE_TIME);
