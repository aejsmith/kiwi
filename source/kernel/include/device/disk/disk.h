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
 * @brief               Disk device class.
 */

#pragma once

#include <device/device.h>

#include <kernel/device/disk.h>

#define DISK_MODULE_NAME "disk"

struct disk_device;

/** Disk device operations. */
typedef struct disk_device_ops {
    /** Destroy the device.
     * @param device        Device to destroy. */
    void (*destroy)(struct disk_device *device);
} disk_device_ops_t;

/** Disk device structure. */
typedef struct disk_device {
    device_t *node;                     /**< Device tree node. */

    disk_device_ops_t *ops;
} disk_device_t;

/** Destroys a disk device.
 * @see                 device_destroy().
 * @param device        Device to destroy. */
static inline status_t disk_device_destroy(disk_device_t *device) {
    return device_destroy(device->node);
}

extern status_t disk_device_create_etc(disk_device_t *device, const char *name, device_t *parent);
extern status_t disk_device_create(disk_device_t *device, device_t *parent);
extern status_t disk_device_publish(disk_device_t *device);
