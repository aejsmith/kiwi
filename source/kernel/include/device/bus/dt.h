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

#include <lib/avl_tree.h>
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

/** Stage at which built-in drivers are initialised. */
typedef enum builtin_dt_driver_type {
    BUILTIN_DT_DRIVER_IRQ,
    BUILTIN_DT_DRIVER_TIME,
} builtin_dt_driver_type_t;

/** DT driver structure. */
typedef struct dt_driver {
    // TODO: bus_driver_t

    dt_match_table_t matches;

    list_t builtin_link;
    builtin_dt_driver_type_t builtin_type;

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
 * They are registered with a stage to run in. During that stage, any devices
 * that match the driver will have their init_builtin() method called.
 *
 * Devices using built-in drivers are still later instantiated as proper
 * devices in the kernel device tree.
 *
 * @param driver            Driver structure.
 */
#define BUILTIN_DT_DRIVER(driver) \
    static __init_text void driver##_builtin_init(void) { \
        dt_register_builtin_driver(&driver); \
    } \
    INITCALL_TYPE(driver##_builtin_init, INITCALL_TYPE_EARLY_DEVICE)

extern void dt_register_builtin_driver(dt_driver_t *driver);

/** DT device flags. */
enum {
    /** Device is marked as available via its status property. */
    DT_DEVICE_AVAILABLE     = (1<<0),

    /** Device is matched to a driver. */
    DT_DEVICE_MATCHED       = (1<<1),
};

/** DT device structure. */
typedef struct dt_device {
    // TODO: bus_device_t.

    int fdt_offset;                 /**< Offset of the corresponding FDT node. */
    uint32_t phandle;               /**< Device node's phandle. */
    const char *name;               /**< Name of the device. */
    const char *compatible;         /**< Compatible string. */
    uint32_t flags;                 /**< Device flags. */

    avl_tree_node_t phandle_link;   /** Link to phandle lookup tree. */

    /** Parent/child tree. */
    struct dt_device *parent;
    list_t parent_link;
    list_t children;

    /** IRQ state. */
    struct dt_device *irq_parent;

    /** Driver state. */
    dt_driver_t *driver;
    dt_match_t *match;
} dt_device_t;

extern dt_device_t *dt_device_get_by_phandle(uint32_t phandle);

/**
 * FDT access.
 */

extern bool dt_get_prop(dt_device_t *device, const char *name, const uint32_t **_value, uint32_t *_len);
extern bool dt_get_prop_u32(dt_device_t *device, const char *name, uint32_t *_value);
#define dt_get_prop_phandle dt_get_prop_u32

extern const void *dt_fdt_get(void);

