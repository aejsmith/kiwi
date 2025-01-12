/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               MBR partition support.
 */

#include <mm/malloc.h>

#include <status.h>

#include "disk.h"
#include "mbr.h"

/** Read in an MBR and convert endianness. */
static status_t read_mbr(disk_device_t *device, object_handle_t *handle, mbr_t *mbr, uint32_t lba) {
    size_t bytes;
    status_t ret = file_read(handle, mbr, sizeof(*mbr), (uint64_t)lba * device->block_size, &bytes);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (bytes != sizeof(*mbr)) {
        /* Corrupt partition table outside of device? */
        return STATUS_NOT_FOUND;
    }

    for (size_t i = 0; i < array_size(mbr->partitions); i++) {
        mbr->partitions[i].start_lba   = le32_to_cpu(mbr->partitions[i].start_lba);
        mbr->partitions[i].num_sectors = le32_to_cpu(mbr->partitions[i].num_sectors);
    }

    return ret;
}

static bool is_partition_valid(disk_device_t *device, mbr_partition_t *partition) {
    return
        (partition->type != 0) &&
        (partition->bootable == 0 || partition->bootable == 0x80) &&
        (partition->start_lba != 0) &&
        (partition->start_lba < device->block_count) &&
        (partition->start_lba + partition->num_sectors <= device->block_count);
}

static bool is_partition_extended(mbr_partition_t *partition) {
    switch (partition->type) {
        case 0x5:
        case 0xf:
        case 0x85:
            /* These are different types of extended partition, 0x5 is
             * supposedly with CHS addressing, while 0xF is LBA. However, Linux
             * treats them the exact same way. */
            return true;
        default:
            return false;
    }
}

static void iterate_extended(disk_device_t *device, object_handle_t *handle, partition_iterate_cb_t cb, uint32_t lba) {
    mbr_t *ebr __cleanup_kfree = kmalloc(sizeof(*ebr), MM_KERNEL);

    size_t id = 4;
    for (uint32_t curr_ebr = lba, next_ebr = 0; curr_ebr != 0; curr_ebr = next_ebr) {
        status_t ret = read_mbr(device, handle, ebr, curr_ebr);
        if (ret != STATUS_SUCCESS) {
            device_kprintf(device->node, LOG_WARN, "failed to read EBR at %" PRIu32 " from device: %d\n", curr_ebr, ret);
            break;
        }

        if (ebr->signature != MBR_SIGNATURE) {
            device_kprintf(device->node, LOG_WARN, "invalid EBR signature, partition table is corrupt\n");
            break;
        }

        /* First entry contains the logical partition, second entry refers to
         * the next EBR, forming a linked list of EBRs. */
        mbr_partition_t *partition = &ebr->partitions[0];
        mbr_partition_t *next      = &ebr->partitions[1];

        /* Calculate the location of the next EBR. The start sector is relative
         * to the start of the extended partition. Set to 0 if the second
         * partition doesn't refer to another EBR entry, causes the loop to end. */
        next->start_lba += lba;
        next_ebr = (is_partition_valid(device, next) && is_partition_extended(next) && next->start_lba > curr_ebr)
            ? next->start_lba
            : 0;

        /* Get the partition. Here the start sector is relative to the current
         * EBR's location. */
        partition->start_lba += curr_ebr;
        if (!is_partition_valid(device, partition))
            continue;

        cb(device, id, partition->start_lba, partition->num_sectors);
        id++;
    }
}

static bool mbr_partition_iterate(disk_device_t *device, object_handle_t *handle, partition_iterate_cb_t cb) {
    mbr_t *mbr __cleanup_kfree = kmalloc(sizeof(*mbr), MM_KERNEL);

    status_t ret = read_mbr(device, handle, mbr, 0);
    if (ret != STATUS_SUCCESS) {
        if (ret != STATUS_NOT_FOUND)
            device_kprintf(device->node, LOG_WARN, "failed to read MBR from device: %d\n", ret);

        return false;
    }

    if (mbr->signature != MBR_SIGNATURE)
        return false;

    /* Check if this is a GPT partition table (technically we should not get
     * here if this is a GPT disk as the GPT code should be reached first). This
     * is just a safeguard. */
    if (mbr->partitions[0].type == MBR_PARTITION_TYPE_GPT)
        return false;

    /* Loop through all partitions in the table. */
    bool seen_extended = false;
    for (size_t i = 0; i < array_size(mbr->partitions); i++) {
        mbr_partition_t *partition = &mbr->partitions[i];

        if (!is_partition_valid(device, partition))
            continue;

        if (is_partition_extended(partition)) {
            if (seen_extended) {
                device_kprintf(device->node, LOG_WARN, "ignoring multiple extended partitions in MBR\n");
                continue;
            }

            iterate_extended(device, handle, cb, partition->start_lba);
            seen_extended = true;
        } else {
            cb(device, i, partition->start_lba, partition->num_sectors);
        }
    }

    return true;
}

const partition_ops_t mbr_partition_ops = {
    .name    = "MBR",
    .iterate = mbr_partition_iterate,
};
