/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Disk device class internal definitions.
 */

#pragma once

#include <device/disk/disk.h>

#include <device/class.h>

extern device_class_t disk_device_class;
extern const device_ops_t disk_device_ops;

extern const disk_device_ops_t partition_device_ops;

static inline bool disk_device_is_partition(disk_device_t *device) {
    return device->ops == &partition_device_ops;
}

/** Partition map iteration callback function type.
 * @param device        Device containing the partition.
 * @param id            ID of the partition.
 * @param lba           Start LBA.
 * @param blocks        Size in blocks. */
typedef void (*partition_iterate_cb_t)(disk_device_t *device, uint8_t id, uint64_t lba, uint64_t blocks);

/** Partition operations. */
typedef struct partition_ops {
    const char *name;                   /**< Name of the partition scheme. */

    /** Iterate over the partitions on a disk.
     * @param device        Disk to iterate over.
     * @param handle        Handle to the disk.
     * @param cb            Callback function.
     * @return              Whether the disk contained a partition map of this
     *                      type. */
    bool (*iterate)(disk_device_t *device, object_handle_t *handle, partition_iterate_cb_t cb);
} partition_ops_t;

extern const partition_ops_t gpt_partition_ops;
extern const partition_ops_t mbr_partition_ops;

extern void partition_probe(disk_device_t *device);

