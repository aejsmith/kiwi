/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               GPT partition support.
 */

#include <lib/string.h>

#include <mm/malloc.h>

#include <status.h>

#include "disk.h"
#include "gpt.h"
#include "mbr.h"

static bool gpt_partition_iterate(disk_device_t *device, object_handle_t *handle, partition_iterate_cb_t cb) {
    status_t ret;

    void *buf __cleanup_kfree = kmalloc(device->block_size, MM_KERNEL);

    /* GPT requires a protective MBR in the first block. Read this in first and
     * check that it contains a protective GPT partition. If we have a legacy
     * MBR then let it be handled through the MBR code. Note that on some
     * systems (e.g. Macs) we can have a "hybrid MBR" where we have both a
     * valid (non-protective) MBR and a GPT. In this case we will use the MBR,
     * since the two should be in sync. */
    mbr_t *mbr = buf;
    size_t bytes;
    ret = file_read(handle, mbr, device->block_size, 0, &bytes);
    if (ret != STATUS_SUCCESS || bytes != device->block_size) {
        device_kprintf(device->node, LOG_WARN, "failed to read GPT from device: %d\n", ret);
        return false;
    } else if (mbr->signature != MBR_SIGNATURE || mbr->partitions[0].type != MBR_PARTITION_TYPE_GPT) {
        return false;
    }

    /* Read in the GPT header (second block). At most one block in size. */
    gpt_header_t *header = buf;
    ret = file_read(handle, header, device->block_size, device->block_size, &bytes);
    if (ret != STATUS_SUCCESS || bytes != device->block_size) {
        device_kprintf(device->node, LOG_WARN, "failed to read GPT from device: %d\n", ret);
        return false;
    } else if (le64_to_cpu(header->signature) != GPT_HEADER_SIGNATURE) {
        return false;
    }

    /* Pull needed information out of the header. */
    uint64_t offset      = le64_to_cpu(header->partition_entry_lba) * device->block_size;
    uint32_t num_entries = le32_to_cpu(header->num_partition_entries);
    uint32_t entry_size  = le32_to_cpu(header->partition_entry_size);

    if (entry_size > device->block_size) {
        device_kprintf(device->node, LOG_WARN, "GPT has entry size larger than block size\n");
        return false;
    }

    gpt_guid_t zero_guid = {};

    /* Iterate over partition entries. */
    for (uint32_t i = 0; i < num_entries; i++, offset += entry_size) {
        gpt_partition_entry_t *entry = buf;
        ret = file_read(handle, entry, entry_size, offset, &bytes);
        if (ret != STATUS_SUCCESS || bytes != entry_size) {
            device_kprintf(device->node, LOG_WARN, "failed to read GPT partition entry at %" PRIu64 ": %d\n", offset, ret);
            return false;
        }

        /* Ignore unused entries. */
        if (memcmp(&entry->type_guid, &zero_guid, sizeof(entry->type_guid)) == 0)
            continue;

        uint64_t lba    = le64_to_cpu(entry->start_lba);
        uint64_t blocks = (le64_to_cpu(entry->last_lba) - lba) + 1;

        if (lba >= device->block_count || lba + blocks > device->block_count) {
            device_kprintf(device->node, LOG_WARN, "GPT partition %" PRIu32 " is outside range of device", i);
            continue;
        }

        cb(device, i, lba, blocks);
    }

    return true;
}

const partition_ops_t gpt_partition_ops = {
    .name    = "GPT",
    .iterate = gpt_partition_iterate,
};
