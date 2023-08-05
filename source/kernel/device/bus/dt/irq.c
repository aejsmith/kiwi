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

#include <mm/malloc.h>

#include <module.h>
#include <status.h>

#include "dt.h"

typedef struct dt_irq_controller {
    list_t link;
    dt_device_t *device;
} dt_irq_controller_t;

typedef struct dt_irq_init {
    list_t controllers;
} dt_irq_init_t;

static __init_text void init_device_irq(dt_device_t *device, void *_init) {
    dt_irq_init_t *init = _init;

    /* Figure out this device's interrupt parent. */
    dt_device_t *parent = device;
    do {
        uint32_t parent_phandle;
        if (dt_get_prop_phandle(parent, "interrupt-parent", &parent_phandle)) {
            parent = dt_device_get_by_phandle(parent_phandle);
        } else {
            parent = parent->parent;
        }
    } while (parent && !dt_get_prop(parent, "#interrupt-cells", NULL, NULL));
    device->irq_parent = (parent != device) ? parent : NULL;

    if (dt_get_prop(device, "interrupt-controller", NULL, NULL)) {
        dt_irq_controller_t *controller = kmalloc(sizeof(*controller), MM_BOOT);

        list_init(&controller->link);
        controller->device = device;

        list_append(&init->controllers, &controller->link);

        dt_match_builtin_driver(device, BUILTIN_DT_DRIVER_IRQ);
    }
}

static void init_irq_controllers(dt_irq_init_t *init, dt_device_t *parent) {
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
                kprintf(LOG_ERROR, "dt: failed to initialise IRQ controller %s: %d\n", device->name, ret);
                dt_device_unmatch(device);
            }

            kfree(controller);
        }
    }
    // TODO: free
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

        symbol_t sym;
        symbol_from_addr((ptr_t)device->driver, &sym, NULL);

        kprintf(
            LOG_DEBUG, "  %s (parent: %s, driver: %s)\n",
            device->name, (device->irq_parent) ? device->irq_parent->name : "none",
            sym.name);
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
