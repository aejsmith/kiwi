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
 * @brief               BCM2836 L1 IRQ controller driver.
 */

#include <device/bus/dt.h>
#include <device/irq.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

typedef struct bcm2836_l1_irq_device {
    io_region_t io;
} bcm2836_l1_irq_device_t;

static bool bcm2836_l1_irq_pre_handle(uint32_t num) {
    assert(false);
    return true;
}

static void bcm2836_l1_irq_post_handle(uint32_t num, bool disable) {
    assert(false);
}

static irq_mode_t bcm2836_l1_irq_mode(uint32_t num) {
    assert(false);
}

static void bcm2836_l1_irq_enable(uint32_t num) {
    assert(false);
}

static void bcm2836_l1_irq_disable(uint32_t num) {
    assert(false);
}

static irq_controller_t bcm2836_l1_irq_controller = {
    .pre_handle  = bcm2836_l1_irq_pre_handle,
    .post_handle = bcm2836_l1_irq_post_handle,
    .mode        = bcm2836_l1_irq_mode,
    .enable      = bcm2836_l1_irq_enable,
    .disable     = bcm2836_l1_irq_disable,
};

static status_t bcm2836_l1_irq_init_builtin(dt_device_t *dt) {
    kprintf(LOG_DEBUG, "hello world\n");

    bcm2836_l1_irq_device_t *device = kmalloc(sizeof(*device), MM_BOOT | MM_ZERO);
    dt->private = device;

    status_t ret = dt_reg_map(dt, 0, MM_BOOT, &device->io);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_ERROR, "bcm2836_l1_irq: failed to map registers: %d\n", ret);
        return ret;
    }

    return STATUS_SUCCESS;
}

static dt_match_t bcm2836_l1_irq_matches[] = {
    { .compatible = "brcm,bcm2836-l1-intc" },
};

static dt_driver_t bcm2836_l1_irq_driver = {
    .matches      = DT_MATCH_TABLE(bcm2836_l1_irq_matches),
    .builtin_type = BUILTIN_DT_DRIVER_IRQ,
    .init_builtin = bcm2836_l1_irq_init_builtin,
};

BUILTIN_DT_DRIVER(bcm2836_l1_irq_driver);
