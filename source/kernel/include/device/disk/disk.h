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
 * @brief               Disk device class.
 */

#pragma once

#include <device/device.h>
#include <device/dma.h>

#include <kernel/device/disk.h>

#define DISK_MODULE_NAME "disk"

struct disk_device;

/** Disk device operations. */
typedef struct disk_device_ops {
    /** Destroy the device.
     * @param device        Device to destroy. */
    void (*destroy)(struct disk_device *device);

    /** Read blocks from the device.
     * @param device        Device to read from.
     * @param buf           Buffer to read into.
     * @param dma           For devices with DISK_DEVICE_DMA set, the DMA
     *                      address of the buffer.
     * @param lba           Block number to start from.
     * @param count         Number of blocks to read.
     * @return              Status code describing result of operation. */
    status_t (*read_blocks)(
        struct disk_device *device, void *buf, dma_ptr_t dma, uint64_t lba,
        size_t count);

    /** Write blocks to the device.
     * @param device        Device to write to.
     * @param buf           Buffer to write from.
     * @param dma           For devices with DISK_DEVICE_DMA set, the DMA
     *                      address of the buffer.
     * @param lba           Block number to start from.
     * @param count         Number of blocks to write.
     * @return              Status code describing result of operation. */
    status_t (*write_blocks)(
        struct disk_device *device, const void *buf, dma_ptr_t dma, uint64_t lba,
        size_t count);
} disk_device_ops_t;

/** Disk device structure. */
typedef struct disk_device {
    device_t *node;                     /**< Device tree node. */

    /** Fields to be filled in by the driver. */
    const disk_device_ops_t *ops;
    uint32_t physical_block_size;       /**< Block size of the underlying disk. */
    uint32_t block_size;                /**< Block size used for I/O. */
    uint64_t block_count;               /**< Number of logical blocks on the device. */
    uint32_t flags;                     /**< Behaviour flags for the device. */
    dma_constraints_t dma_constraints;  /**< DMA constraints (if DISK_DEVICE_DMA set). */

    /** Internal fields. */
    uint64_t size;                      /**< Total size of the device. */
} disk_device_t;

/** Disk device flags. */
enum {
    /**
     * Device requires DMA-accessible memory for block transfers. Memory will
     * satisfy the constraints given in the device.
     */
    DISK_DEVICE_DMA = (1<<0),
};

/** Destroys a disk device.
 * @see                 device_destroy().
 * @param device        Device to destroy. */
static inline status_t disk_device_destroy(disk_device_t *device) {
    return device_destroy(device->node);
}

extern status_t disk_device_create_etc(disk_device_t *device, const char *name, device_t *parent);
extern status_t disk_device_create(disk_device_t *device, device_t *parent);
extern void disk_device_publish(disk_device_t *device);
