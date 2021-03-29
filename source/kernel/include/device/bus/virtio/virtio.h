/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               VirtIO bus manager.
 */

#pragma once

#include <device/bus.h>

#include <device/bus/virtio/virtio_ids.h>
#include <device/bus/virtio/virtio_ring.h>

#include <kernel/device/bus/virtio.h>

struct virtio_device;

#define VIRTIO_MODULE_NAME "virtio"

extern bus_t virtio_bus;

/** VirtIO driver structure. */
typedef struct virtio_driver {
    bus_driver_t bus;

    uint16_t device_id;                     /**< Supported device ID. */

    /** Initialize a device that matched against this driver.
     * @param device        Device to initialize.
     * @return              Status code describing the result of the operation. */
    status_t (*init_device)(struct virtio_device *device);
} virtio_driver_t;

/** Define module init/unload functions for a VirtIO driver.
 * @param driver        Driver to register. */
#define MODULE_VIRTIO_DRIVER(driver) \
    MODULE_BUS_DRIVER(virtio_bus, driver)

/** VirtIO device structure. */
typedef struct virtio_device {
    bus_device_t bus;

    /** To be filled in by the transport on initialization. */
    uint16_t device_id;                     /**< Device ID. */
} virtio_device_t;

extern status_t virtio_create_device_impl(module_t *module, device_t *parent, virtio_device_t *device);

/** @see virtio_create_device_impl() */
#define virtio_create_device(parent, device) \
    virtio_create_device_impl(module_self(), parent, device)
