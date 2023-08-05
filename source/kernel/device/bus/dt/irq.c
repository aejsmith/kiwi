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

        kprintf(LOG_DEBUG, "  %s (parent: %s)\n", device->name, (device->irq_parent) ? device->irq_parent->name : "none");
    }

    // TODO: free the controller list
}

INITCALL_TYPE(dt_irq_init, INITCALL_TYPE_IRQ);
