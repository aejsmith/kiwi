/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device bus management.
 */

#pragma once

#include <device/device.h>

#include <status.h>

struct bus_device;
struct bus_driver;

/** Bus type definition structure. */
typedef struct bus_type {
    const char *name;                   /**< Name of the bus. */

    /**
     * Class name (attribute value) used for device tree nodes representing
     * devices attached to the bus.
     */
    const char *device_class;

    /**
     * Function called to match devices against a driver. This is called in
     * 2 situations:
     *
     *  - A new device is created on the bus, in which case this will be called
     *    on that device for all drivers registered for the bus to find a driver
     *    for the device.
     *  - A new driver is registered on the bus, in which case this will be
     *    called for that driver on all devices under the bus root directory
     *    whose class matches the device_class string, so that newly loaded
     *    drivers can find devices which they support.
     *
     * @param device        Device to match.
     * @param driver        Driver to match.
     *
     * @return              Whether the device and driver match.
     */
    bool (*match_device)(struct bus_device *device, struct bus_driver *driver);

    /** Initialize a device once a match has been found.
     * @param device        Device to match.
     * @param driver        Driver to match.
     * @return              Status code describing the result of the operation. */
    status_t (*init_device)(struct bus_device *device, struct bus_driver *driver);
} bus_type_t;

/** Device bus structure. */
typedef struct bus {
    bus_type_t *type;
    device_t *dir;
    mutex_t lock;                       /**< Lock for driver list. */
    list_t drivers;                     /**< List of drivers for the bus. */
} bus_t;

/**
 * Bus driver header structure. This is intended to be embedded inside of a
 * bus-specific driver structure including any information needed to identify
 * the devices that are supported by the driver.
 */
typedef struct bus_driver {
    list_t link;                        /**< Link to drivers list. */
} bus_driver_t;

/**
 * Bus device header structure. This is intended to be embedded inside of a
 * bus-specific device structure.
 */
typedef struct bus_device {
    bus_driver_t *driver;               /**< Driver which manages the device. */
    device_t *node;                     /**< Device tree node. */
} bus_device_t;

extern status_t bus_init(bus_t *bus, bus_type_t *type);
extern status_t bus_destroy(bus_t *bus);

extern status_t bus_register_driver(bus_t *bus, bus_driver_t *driver);
extern status_t bus_unregister_driver(bus_t *bus, bus_driver_t *driver);

extern status_t bus_create_device(
    bus_t *bus, bus_device_t *device, const char *name, device_ops_t *ops,
    device_attr_t *attrs, size_t count);
extern void bus_match_device(bus_t *bus, bus_device_t *device);

extern void bus_device_init(bus_device_t *device);

/** Destroy a bus device. */
static inline void bus_device_destroy(bus_device_t *device) {
    fatal("TODO");
}

/** Define module init/unload functions for a bus driver.
 * @param bus           Bus to register to.
 * @param driver        Driver to register. */
#define MODULE_BUS_DRIVER(bus, driver) \
    static status_t driver##_init(void) { \
        return bus_register_driver(&bus, (bus_driver_t *)&driver); \
    } \
    static status_t driver##_unload(void) { \
        return bus_unregister_driver(&bus, (bus_driver_t *)&driver); \
    } \
    MODULE_FUNCS(driver##_init, driver##_unload)
