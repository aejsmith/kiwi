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
 * @brief               DT IRQ handling.
 */

#include <device/irq.h>

#include <mm/malloc.h>

#include <status.h>

#include "dt.h"

typedef struct dt_irq_controller {
    list_t link;
    dt_device_t *device;
} dt_irq_controller_t;

typedef struct dt_irq_init {
    list_t controllers;
} dt_irq_init_t;

static status_t dt_device_irq_translate(
    irq_domain_t *domain, uint32_t num, irq_domain_t **_dest_domain,
    uint32_t *_dest_num)
{
    dt_device_t *device = domain->private;
    dt_device_t *parent = device->irq_parent;

    assert(parent);

    *_dest_domain = parent->irq_controller.domain;
    *_dest_num    = parent->irq_controller.ops->translate(parent, device, num);

    return (*_dest_num != UINT32_MAX) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

static irq_domain_ops_t dt_device_irq_ops = {
    .translate = dt_device_irq_translate,
};

/** Gets an interrupt specifier from a device's interrupts property. */
bool dt_irq_get_prop(dt_device_t *device, uint32_t num, uint32_t *_value) {
    dt_device_t *parent = device->irq_parent;

    const uint32_t *irqs_val;
    uint32_t irqs_len;
    if (parent && dt_get_prop(device, "interrupts", &irqs_val, &irqs_len)) {
        /* Validity of property has been checked in init_device_irq(). */
        uint32_t irqs_count = irqs_len / 4 / parent->irq_controller.num_cells;
        if (num < irqs_count) {
            for (uint32_t i = 0; i < parent->irq_controller.num_cells; i++)
                _value[i] = fdt32_to_cpu(irqs_val[(num * parent->irq_controller.num_cells) + i]);

            return true;
        }
    }

    return false;
}

/** Converts standard (Linux-style) DT IRQ modes to our own. */
irq_mode_t dt_irq_mode(uint32_t mode) {
    switch (mode) {
        case 1:
            /* IRQ_TYPE_EDGE_RISING */
            return IRQ_MODE_EDGE;
        case 4:
            /* IRQ_TYPE_LEVEL_HIGH */
            return IRQ_MODE_LEVEL;
        default:
            kprintf(LOG_ERROR, "dt: unsupported IRQ mode %u\n", mode);
            return IRQ_MODE_EDGE;
    }
}

static void dt_irq_two_cell_configure(dt_device_t *controller, dt_device_t *child, uint32_t num) {
    uint32_t prop[2];
    bool success __unused = dt_irq_get_prop(child, num, prop);
    assert(success);

    irq_mode_t mode = dt_irq_mode(prop[1] & 0xf);
    status_t ret = irq_set_mode(controller->irq_controller.domain, prop[0], mode);
    if (ret != STATUS_SUCCESS) {
        kprintf(
            LOG_ERROR, "dt: failed to set mode %d for interrupt %u in device %s (dest_num: %u)\n",
            prop[1], num, child->name, prop[0]);
    }
}

static uint32_t dt_irq_two_cell_translate(dt_device_t *controller, dt_device_t *child, uint32_t num) {
    uint32_t prop[2];
    bool success = dt_irq_get_prop(child, num, prop);
    return (success) ? prop[0] : UINT32_MAX;
}

dt_irq_ops_t dt_irq_two_cell_ops = {
    .configure = dt_irq_two_cell_configure,
    .translate = dt_irq_two_cell_translate,
};

/** Configures IRQs for a device after matching to a driver. */
bool dt_irq_init_device(dt_device_t *device) {
    dt_device_t *parent = device->irq_parent;

    uint32_t irqs_len;
    if (parent && dt_get_prop(device, "interrupts", NULL, &irqs_len)) {
        /* Validity of property has been checked in init_device_irq(). */
        uint32_t irqs_count = irqs_len / 4 / parent->irq_controller.num_cells;

        if (irqs_count > 0) {
            device->irq_domain = irq_domain_create(irqs_count, &dt_device_irq_ops, device);

            if (parent->irq_controller.ops->configure) {
                for (uint32_t i = 0; i < irqs_count; i++)
                    parent->irq_controller.ops->configure(parent, device, i);
            }
        }
    }

    return true;
}

/** Destroys IRQ state for a device. */
void dt_irq_deinit_device(dt_device_t *device) {
    if (device->irq_domain) {
        //destroy
    }
}

/** Sets the IRQ controller properties of the controller's DT node.
 * @param device            IRQ controller node.
 * @param domain            IRQ controller's domain.
 * @param ops               Operations for setting up child devices. */
void dt_irq_init_controller(dt_device_t *device, irq_domain_t *domain, dt_irq_ops_t *ops) {
    assert(!device->irq_controller.domain);

    device->irq_controller.domain = domain;
    device->irq_controller.ops    = ops;
}

/**
 * Registers an IRQ handler for a DT device. The given IRQ number is the index
 * within the interrupts property of the device. The handler should be removed
 * with irq_unregister() when no longer needed.
 *
 * @see                 irq_register().
 *
 * @param device        DT device to register for.
 * @param num           Index of the IRQ within the interrupts property of the
 *                      device.
 */
status_t dt_irq_register(
    dt_device_t *device, uint32_t num, irq_early_func_t early_func,
    irq_func_t func, void *data, irq_handler_t **_handler)
{
    return irq_register(device->irq_domain, num, early_func, func, data, _handler);
}

static __init_text void init_device_irq(dt_device_t *device, void *_init) {
    dt_irq_init_t *init = _init;

    /* Figure out this device's interrupt parent. */
    dt_device_t *parent = device;
    uint32_t num_cells;
    do {
        uint32_t parent_phandle;
        if (dt_get_prop_phandle(parent, "interrupt-parent", &parent_phandle)) {
            parent = dt_device_get_by_phandle(parent_phandle);
        } else {
            parent = parent->parent;
        }
    } while (parent && !dt_get_prop_u32(parent, "#interrupt-cells", &num_cells));
    device->irq_parent = (parent != device) ? parent : NULL;

    if (device->irq_parent) {
        /* Validate interrupts property against interrupt-cells. */
        uint32_t irqs_len;
        if (dt_get_prop(device, "interrupts", NULL, &irqs_len)) {
            if ((irqs_len / 4) % num_cells) {
                kprintf(LOG_ERROR, "dt: %s has invalid interrupts property length\n", device->name);
                device->irq_parent = NULL;
            }
        }

        // TODO: Interrupt nexuses.
        if (dt_get_prop(device->irq_parent, "interrupt-map", NULL, NULL)) {
            kprintf(LOG_ERROR, "dt: %s has interrupt nexus as IRQ parent, this is not currently supported\n", device->name);
            device->irq_parent = NULL;
        }
    }

    if (dt_get_prop(device, "interrupt-controller", NULL, NULL)) {
        device->irq_controller.num_cells = num_cells;

        dt_irq_controller_t *controller = kmalloc(sizeof(*controller), MM_BOOT);

        list_init(&controller->link);
        controller->device = device;

        list_append(&init->controllers, &controller->link);

        dt_match_builtin_driver(device, BUILTIN_DT_DRIVER_IRQ);
    }
}

static __init_text void init_irq_controllers(dt_irq_init_t *init, dt_device_t *parent) {
    list_foreach_safe(&init->controllers, iter) {
        dt_irq_controller_t *controller = list_entry(iter, dt_irq_controller_t, link);
        dt_device_t *device = controller->device;

        if (device->irq_parent == parent && device->flags & DT_DEVICE_MATCHED) {
            list_remove(&controller->link);

            status_t ret = device->driver->init_builtin(device);
            if (ret == STATUS_SUCCESS) {
                /* Initialise child controllers. */
                init_irq_controllers(init, device);
            } else {
                fatal("Failed to initialise IRQ controller %s: %d\n", device->name, ret);
            }

            kfree(controller);
        }
    }
}

/** Initialise DT IRQ devices. */
static __init_text void dt_irq_init(void) {
    /*
     * Traverse the device tree to set up IRQ information, and gather a list of
     * interrupt controllers to initialise.
     *
     * We need to initialise controllers in order of their specified hierarchy
     * via interrupt-parent. This hierarchy is not necessarily the same as the
     * node hierarchy. So, we collect a list of them here, then figure out the
     * order to initialise in.
     */
    dt_irq_init_t init;
    list_init(&init.controllers);
    dt_iterate(init_device_irq, &init);

    kprintf(LOG_DEBUG, "dt: found IRQ controllers:\n");

    list_foreach(&init.controllers, iter) {
        dt_irq_controller_t *controller = list_entry(iter, dt_irq_controller_t, link);
        dt_device_t *device = controller->device;

        kprintf(
            LOG_DEBUG, "  %s (parent: %s, driver: %s)\n",
            device->name, (device->irq_parent) ? device->irq_parent->name : "none",
            dt_get_builtin_driver_name(device->driver));
    }

    /*
     * Initialise all controllers without a parent first, recursing down into
     * those that are parented to each of those.
     */
    init_irq_controllers(&init, NULL);

    if (!list_empty(&init.controllers)) {
        kprintf(LOG_WARN, "dt: could not initialise all IRQ controllers:\n");

        while (!list_empty(&init.controllers)) {
            dt_irq_controller_t *controller = list_first(&init.controllers, dt_irq_controller_t, link);

            kprintf(LOG_WARN, "  %s\n", controller->device->name);

            dt_device_unmatch(controller->device);

            list_remove(&controller->link);
            kfree(controller);
        }
    }
}

INITCALL_TYPE(dt_irq_init, INITCALL_TYPE_IRQ);
