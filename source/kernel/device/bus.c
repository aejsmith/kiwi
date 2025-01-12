/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device bus management.
 */

#include <device/bus.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/** Initializes a bus.
 * @param bus           Bus to initialize.
 * @param type          Type of the bus.
 * @return              Status code describing the result of the operation. */
status_t bus_init(bus_t *bus, bus_type_t *type) {
    mutex_init(&bus->lock, "bus_lock", 0);
    list_init(&bus->drivers);

    bus->type = type;

    status_t ret = device_create_etc(module_caller(), type->name, device_bus_dir, NULL, NULL, NULL, 0, &bus->dir);
    if (ret != STATUS_SUCCESS)
        return ret;

    device_publish(bus->dir);

    return ret;
}

/** Destroys a bus.
 * @param bus           Bus to destroy.
 * @return              Status code describing the result of the operation. */
status_t bus_destroy(bus_t *bus) {
    return STATUS_NOT_IMPLEMENTED;
}

static bool match_device(bus_t *bus, bus_device_t *device, bus_driver_t *driver) {
    bool match = bus->type->match_device(device, driver);
    if (match) {
        device->driver = driver;

        status_t ret = bus->type->init_device(device, driver);
        if (ret != STATUS_SUCCESS) {
            device_kprintf(
                device->node, LOG_WARN, "failed to initialize device: %" PRId32 "\n",
                ret);
        }
    }

    return match;
}

typedef struct match_tree_device_data {
    bus_t *bus;
    bus_driver_t *driver;
    char *attr;
} match_tree_device_data_t;

static int match_tree_device(device_t *device, void *_data) {
    match_tree_device_data_t *data = _data;
    status_t ret;

    ret = device_attr(device, DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, data->attr, DEVICE_ATTR_MAX, NULL);
    if (ret != STATUS_SUCCESS || strcmp(data->attr, data->bus->type->device_class) != 0) {
        /* Do not try to match nodes whose class is not the correct one, but do
         * descend into it. This allows bus managers to implement a more
         * structured tree hierarchy than just dumping all of its device nodes
         * into the single bus directory. */
        return DEVICE_ITERATE_DESCEND;
    } else {
        /* This is a bus device node. Probe it if not already claimed. */
        bus_device_t *bus_device = device->private;
        if (!bus_device->driver)
            match_device(data->bus, bus_device, data->driver);

        /* Don't descend into bus device nodes. We don't care about any device
         * nodes that existing drivers have created under their bus device. */
        return DEVICE_ITERATE_CONTINUE;
    }
}

/**
 * Registers a new bus driver. This will search devices connected to the bus for
 * ones supported by the newly added driver and initialize any found.
 *
 * @param bus           Bus to register to.
 * @param driver        Driver to register.
 *
 * @return              Status code describing the result of the operation.
 */
status_t bus_register_driver(bus_t *bus, bus_driver_t *driver) {
    list_init(&driver->link);

    mutex_lock(&bus->lock);

    list_append(&bus->drivers, &driver->link);

    /* Allocate a string to fetch the attribute name into as the maximum is
     * quite large. */
    match_tree_device_data_t data;
    data.bus    = bus;
    data.driver = driver;
    data.attr   = kmalloc(DEVICE_ATTR_MAX, MM_KERNEL);

    device_iterate(bus->dir, match_tree_device, &data);

    kfree(data.attr);

    mutex_unlock(&bus->lock);

    return STATUS_SUCCESS;
}

/** Unregisters a bus driver.
 * @param bus           Bus to unregister from.
 * @param driver        Driver to unregister.
 * @return              Status code describing the result of the operation. */
status_t bus_unregister_driver(bus_t *bus, bus_driver_t *driver) {
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Initializes a new bus device and creates the device tree node for it under
 * the bus root. This should be called only by the bus manager driver.
 *
 * This is equivalent to calling bus_device_init() followed by device_create().
 * If the bus manager needs more flexibility in creating the device (e.g.
 * different parent), it can call bus_device_init() and then device_create()
 * itself. When doing so, the device private pointer must be set to the
 * bus_device_t, and the device node must be stored in bus_device_t::node.
 *
 * @param bus           Bus to create on.
 * @param device        Bus device to add.
 * @param name          Name for the device tree node.
 * @param ops           Pointer to operations for the device (can be NULL).
 * @param attrs         Optional array of attributes for the device (will be
 *                      duplicated).
 * @param count         Number of attributes.
 * 
 * @see                 device_create_etc().
 *
 * @return              Status code describing the result of the operation.
 */
status_t bus_create_device(
    bus_t *bus, bus_device_t *device, const char *name, device_ops_t *ops,
    device_attr_t *attrs, size_t count)
{
    bus_device_init(device);

    status_t ret = device_create_etc(module_caller(), name, bus->dir, ops, device, attrs, count, &device->node);
    if (ret != STATUS_SUCCESS)
        return ret;

    return STATUS_SUCCESS;
}

/**
 * Indicates that a new device has been added to the bus. This will search
 * currently loaded drivers to find one which supports the device.
 *
 * @param bus           Bus that the device is added to.
 * @param device        Device that has been added. The device tree node should
 *                      have been created. Its private pointer will be set to
 *                      the bus_device_t.
 */
void bus_match_device(bus_t *bus, bus_device_t *device) {
    assert(device->node);
    device->node->private = device;

    mutex_lock(&bus->lock);

    list_foreach(&bus->drivers, iter) {
        bus_driver_t *driver = list_entry(iter, bus_driver_t, link);

        if (match_device(bus, device, driver))
            break;
    }

    mutex_unlock(&bus->lock);
}

/** Initializes a bus device structure.
 * @param device        Device to initialize. */
void bus_device_init(bus_device_t *device) {
    device->driver = NULL;
    device->node   = NULL;
}
