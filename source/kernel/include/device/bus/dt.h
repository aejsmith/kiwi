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

#pragma once

#include <device/io.h>
#include <device/irq.h>

#include <kernel/device/bus/dt.h>

#include <lib/array.h>
#include <lib/avl_tree.h>
#include <lib/utility.h>

#include <kernel.h>

#include <assert.h>
#include <libfdt.h>

struct device;
struct dt_device;
struct dt_irq_ops;

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
    BUILTIN_DT_DRIVER_NONE = 0,

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
    array_t compatible;             /**< Compatible strings. */
    uint32_t flags;                 /**< Device flags. */

    /**
     * Device private pointer. To be used by built-in drivers which need to
     * initialise before the bus manager is set up. Normal drivers should prefer
     * the usual device_t private pointer.
     */
    void *private;

    avl_tree_node_t phandle_link;   /** Link to phandle lookup tree. */

    /** Parent/child tree. */
    struct dt_device *parent;
    list_t parent_link;             /**< Link to parent's children list. */
    list_t children;                /**< List of child nodes. */

    /**
     * Resolved interrupt parent device, from searching the hierarchy to find
     * the interrupt controller/nexus node.
     */
    struct dt_device *irq_parent;

    /**
     * IRQ domain local to this device. Maps indices into the interrupts
     * property on the DT node to the correct IRQ within the interrupt parent.
     */
    irq_domain_t *irq_domain;

    /**
     * For an interrupt controller, the IRQ domain created by the driver that
     * devices whose interrupt parent is set to this controller will use, and
     * operations for setting up IRQs for children of this controller.
     */
    struct {
        irq_domain_t *domain;
        struct dt_irq_ops *ops;
        uint32_t num_cells;
    } irq_controller;

    /** Driver state. */
    dt_driver_t *driver;
    dt_match_t *match;
} dt_device_t;

extern dt_device_t *dt_device_get_by_phandle(uint32_t phandle);

/**
 * FDT access.
 */

/** Gets the number of entries in a property.
 * @param len           Byte length of the property.
 * @param num_cells     Number of cells per entry. */
static inline uint32_t dt_get_num_entries(uint32_t len, uint32_t num_cells) {
    return len / 4 / num_cells;
}

extern uint64_t dt_get_value(const uint32_t *ptr, uint32_t num_cells);

extern bool dt_get_prop(dt_device_t *device, const char *name, const uint32_t **_value, uint32_t *_len);
extern bool dt_get_prop_u32(dt_device_t *device, const char *name, uint32_t *_value);
#define dt_get_prop_phandle dt_get_prop_u32

extern const void *dt_fdt_get(void);

/**
 * Memory access.
 */

extern uint32_t dt_get_address_cells(dt_device_t *device);
extern uint32_t dt_get_size_cells(dt_device_t *device);

extern bool dt_reg_get(dt_device_t *device, uint8_t index, phys_ptr_t *_address, phys_size_t *_size);

extern status_t dt_reg_map(dt_device_t *device, uint8_t index, uint32_t mmflag, io_region_t *_region);
extern status_t dt_reg_map_etc(
    dt_device_t *device, uint8_t index, phys_ptr_t offset, phys_size_t size,
    uint32_t flags, uint32_t mmflag, io_region_t *_region);
extern void dt_reg_unmap(dt_device_t *device, uint8_t index, io_region_t region);
extern void dt_reg_unmap_etc(
    dt_device_t *device, uint8_t index, io_region_t region, phys_ptr_t offset,
    phys_size_t size);

extern status_t device_dt_reg_map(
    struct device *owner, dt_device_t *device, uint8_t index, uint32_t mmflag,
    io_region_t *_region);
extern status_t device_dt_reg_map_etc(
    struct device *owner, dt_device_t *device, uint8_t index, phys_ptr_t offset,
    phys_size_t size, uint32_t flags, uint32_t mmflag, io_region_t *_region);

/**
 * IRQ handling.
 */

/**
 * DT IRQ controller operations. This is needed since the format of the
 * interrupts property of a node is specific to the type of the controller that
 * is its IRQ parent.
 */
typedef struct dt_irq_ops {
    /**
     * Configures an IRQ for a device whose IRQ parent is this controller from
     * its DT node. Should apply things like IRQ mode configuration that are
     * specified in the interrupts property for the node. Called when the
     * device is initially matched to a driver.
     *
     * @param controller    IRQ controller node.
     * @param child         Child node whose IRQ is being registered.
     * @param num           IRQ index within the child.
     */
    void (*configure)(dt_device_t *controller, dt_device_t *child, uint32_t num);

    /**
     * Translates an IRQ number within a child device to the IRQ number within
     * the controller's IRQ domain.
     *
     * @param controller    IRQ controller node.
     * @param child         Child node whose IRQ is being registered.
     * @param num           IRQ index within the child.
     *
     * @return              Translated IRQ number, or UINT32_MAX on failure.
     */
    uint32_t (*translate)(dt_device_t *controller, dt_device_t *child, uint32_t num);
} dt_irq_ops_t;

extern void dt_irq_init_controller(dt_device_t *device, irq_domain_t *domain, dt_irq_ops_t *ops);

extern status_t dt_irq_register(
    dt_device_t *device, uint32_t num, irq_early_func_t early_func,
    irq_func_t func, void *data, irq_handler_t **_handler);

extern bool dt_irq_get_prop(dt_device_t *device, uint32_t num, uint32_t *_value);
extern irq_mode_t dt_irq_mode(uint32_t mode);

extern dt_irq_ops_t dt_irq_two_cell_ops;
