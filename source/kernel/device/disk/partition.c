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
 * @brief               Disk partition support.
 */

#include <lib/string.h>

#include <mm/malloc.h>

#include <status.h>

#include "disk.h"

typedef struct partition_device {
    disk_device_t disk;

    disk_device_t *parent;
    uint64_t offset;
} partition_device_t;

DEFINE_CLASS_CAST(partition_device, disk_device, disk);

static status_t partition_device_read_blocks(
    disk_device_t *_device, void *buf, dma_ptr_t dma, uint64_t lba,
    size_t count)
{
    partition_device_t *device = cast_partition_device(_device);

    if (!device->parent->ops->read_blocks)
        return STATUS_NOT_SUPPORTED;

    return device->parent->ops->read_blocks(device->parent, buf, dma, lba + device->offset, count);
}

static status_t partition_device_write_blocks(
    disk_device_t *_device, const void *buf, dma_ptr_t dma, uint64_t lba,
    size_t count)
{
    partition_device_t *device = cast_partition_device(_device);

    if (!device->parent->ops->write_blocks)
        return STATUS_NOT_SUPPORTED;

    return device->parent->ops->write_blocks(device->parent, buf, dma, lba + device->offset, count);
}

const disk_device_ops_t partition_device_ops = {
    .read_blocks  = partition_device_read_blocks,
    .write_blocks = partition_device_write_blocks,
};

static void add_partition(disk_device_t *parent, uint8_t id, uint64_t lba, uint64_t blocks) {
    status_t ret;

    partition_device_t *device = kmalloc(sizeof(*device), MM_KERNEL | MM_ZERO);

    char name[4];
    snprintf(name, sizeof(name), "%" PRIu8, id);

    ret = device_class_create_device(
        &disk_device_class, module_self(), name, parent->node, &disk_device_ops,
        &device->disk, NULL, 0, DEVICE_CLASS_CREATE_DEVICE_NO_ALIAS,
        &device->disk.node);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(parent->node, LOG_ERROR, "failed to create partition device %" PRIu8 ": %d\n", id, ret);
        kfree(device);
        return;
    }

    device_add_kalloc(device->disk.node, device);

    device->disk.ops                 = &partition_device_ops;
    device->disk.physical_block_size = parent->physical_block_size;
    device->disk.block_size          = parent->block_size;
    device->disk.block_count         = blocks;
    device->disk.flags               = parent->flags;
    device->disk.dma_constraints     = parent->dma_constraints;
    device->parent                   = parent;
    device->offset                   = lba;

    disk_device_publish(&device->disk);

    device_kprintf(
        parent->node, LOG_NOTICE, "partition %" PRIu8 " @ %" PRIu64 ", %" PRIu64 " MiB (blocks: %" PRIu64 ")\n",
        id, lba, (blocks * parent->block_size) / 1024 / 1024, blocks);
}

static const partition_ops_t *partition_types[] = {
    &gpt_partition_ops,
    &mbr_partition_ops,
};

/** Probe for partitions on a disk device. */
void partition_probe(disk_device_t *device) {
    object_handle_t *handle __cleanup_object_handle = NULL;
    status_t ret = device_get(device->node, FILE_ACCESS_READ, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(device->node, LOG_WARN, "failed to open device for partition probe: %d\n", ret);
        return;
    }

    for (size_t i = 0; i < array_size(partition_types); i++) {
        if (partition_types[i]->iterate(device, handle, add_partition)) {
            // TODO: Add partition map type name as an attribute to the parent.
            device_kprintf(
                device->node, LOG_NOTICE, "added partitions from %s partition map\n",
                partition_types[i]->name);

            break;
        }
    }
}
