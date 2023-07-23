/*
 * Copyright (C) 2009-2022 Alex Smith
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

#pragma once

#include <kernel/device/bus/dt.h>

#include <lib/utility.h>

#include <kernel.h>

#include <libfdt.h>

struct dt_device;

/**
 * Driver implementation.
 */

/** Structure defining a compatible string that a driver matches. */
typedef struct dt_match {
    const char *compatible;         /**< Compatible string to match against. */
    const void *private;            /**< Driver private information. */
} dt_match_t;

/** Table of all devices that a DT driver matches. */
typedef struct dt_match_table {
    dt_match_t *array;
    size_t count;
} dt_match_table_t;

/**
 * Initialize a DT match table. This is for use within the definition of the
 * DT driver. Example definition of a match table:
 *
 *   static dt_match_t my_dt_driver_matches[] = {
 *       { .compatible = "test,device-1234", .private = &device_1234_data },
 *       { .compatible = "test,device-5678", .private = &device_5678_data },
 *   };
 *
 *   static dt_driver_t my_dt_driver = {
 *       .matches = DT_MATCH_TABLE(my_dt_driver_matches),
 *       ...
 *   };
 */
#define DT_MATCH_TABLE(table) { table, array_size(table) }

/** DT driver structure. */
typedef struct dt_driver {
    // TODO: bus_driver_t

    dt_match_table_t matches;

    /**
     * Initialisation for builtin drivers for low-level devices. This will be
     * called for any matches during the initcall stage specified in the
     * BUILTIN_DT_DRIVER definition.
     *
     * If an init_device() method is specified, this will also be called later
     * during bus initialisation to set up the full bus device.
     *
     * @param device        DT device that matched this driver.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*init_builtin)(struct dt_device *device);
} dt_driver_t;

/**
 * Define a built-in DT driver. Built-in drivers are used for low-level devices
 * (IRQ controllers, timers, etc.) that are needed earlier in boot before the
 * full device manager is initialised.
 *
 * They are registered with an initcall stage to run in. During that stage,
 * any devices that match the driver will have their init_builtin() method
 * called.
 *
 * Devices using built-in drivers are still later instantiated as proper
 * devices in the kernel device tree.
 *
 * @param driver            Driver structure.
 * @param init_type         Initcall type. Must be later than
 *                          INITCALL_TYPE_EARLY_DEVICE, as the DT initialisation
 *                          does not take place until then.
 */
#define BUILTIN_DT_DRIVER(driver, init_type) \
    static __init_text void driver##_builtin_init(void) { \
        dt_register_builtin_driver(&driver); \
    } \
    INITCALL_TYPE(driver##_builtin_init, init_type)
// TODO: register bus driver

extern void dt_register_builtin_driver(dt_driver_t *driver);

/** DT device structure. */
typedef struct dt_device {
    // TODO: bus_device_t.

    int fdt_offset;                 /**< Offset of the corresponding FDT node. */
} dt_device_t;

/**
 * FDT access.
 */

extern const void *dt_fdt_get(void);

