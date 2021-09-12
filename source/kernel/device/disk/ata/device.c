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
 * @brief               ATA device implementation.
 */

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>
#include <time.h>

#include "ata.h"

enum {
    ATA_MODE_PIO,
    ATA_MODE_MULTIWORD_0,
    ATA_MODE_MULTIWORD_1,
    ATA_MODE_MULTIWORD_2,
    ATA_MODE_UDMA_0,
    ATA_MODE_UDMA_1,
    ATA_MODE_UDMA_2,
    ATA_MODE_UDMA_3,
    ATA_MODE_UDMA_4,
    ATA_MODE_UDMA_5,
    ATA_MODE_UDMA_6,
};

static const char *ata_mode_strings[] = {
    [ATA_MODE_PIO]         = "PIO",
    [ATA_MODE_MULTIWORD_0] = "MWDMA0",
    [ATA_MODE_MULTIWORD_1] = "MWDMA1",
    [ATA_MODE_MULTIWORD_2] = "MWDMA2",
    [ATA_MODE_UDMA_0]      = "UDMA/16",
    [ATA_MODE_UDMA_1]      = "UDMA/25",
    [ATA_MODE_UDMA_2]      = "UDMA/33",
    [ATA_MODE_UDMA_3]      = "UDMA/44",
    [ATA_MODE_UDMA_4]      = "UDMA/66",
    [ATA_MODE_UDMA_5]      = "UDMA/100",
    [ATA_MODE_UDMA_6]      = "UDMA/133",
};

#define LBA28_MAX_BLOCK (1ull<<28)
#define LBA48_MAX_BLOCK (1ull<<48)

/** Array of commands. First index = write, second = LBA48, third = DMA. */
static const uint8_t transfer_commands[2][2][2] = {
    {
        { ATA_CMD_READ_SECTORS, ATA_CMD_READ_DMA },
        { ATA_CMD_READ_SECTORS_EXT, ATA_CMD_READ_DMA_EXT },
    },
    {
        { ATA_CMD_WRITE_SECTORS, ATA_CMD_WRITE_DMA },
        { ATA_CMD_WRITE_SECTORS_EXT, ATA_CMD_WRITE_DMA_EXT },
    },
};

static status_t ata_device_begin_transfer(
    ata_device_t *device, void *buf, uint64_t lba, size_t count, bool write,
    size_t *_transfer_count, bool *_dma)
{
    bool dma = false;//device->caps & ATA_DEVICE_CAP_DMA

    if (lba < LBA28_MAX_BLOCK) {
        /* Check how many blocks we can transfer. */
        if ((lba + count) > LBA28_MAX_BLOCK)
            count = LBA28_MAX_BLOCK - lba;
        if (count > 256)
            count = 256;

        if (dma) {
            /* Prepare the DMA transfer. */
            // TODO
            //ret = ata_channel_prepare_dma(device->channel, buf, count * device->block_size, write);
            //if (ret != STATUS_SUCCESS) {
            //    device_kprintf(device->disk.node, LOG_WARN, "failed to prepare DMA transfer (%d)\n", ret);
            //    return ret;
            //}
        }

        /* Start the transfer. */
        device->channel->ops->lba28_setup(device->channel, device->num, lba, count);
        ata_channel_command(device->channel, transfer_commands[write][false][dma]);
    } else if (lba < LBA48_MAX_BLOCK) {
        if (!(device->caps & ATA_DEVICE_CAP_LBA48)) {
            device_kprintf(
                device->disk.node, LOG_WARN, "attempted LBA48 read (%" PRIu64 ") on non-LBA48 device\n",
                lba);
            return STATUS_DEVICE_ERROR;
        }

        /* Check how many blocks we can transfer. FIXME: Because I'm lazy and
         * haven't made ata_channel_prepare_dma() handle the case where we have
         * more than the maximum number of blocks per transfer, limit this to
         * 256. It can actually be 65536. */
        if ((lba + count) > LBA48_MAX_BLOCK)
            count = LBA48_MAX_BLOCK - lba;
        if (count > 256)
            count = 256;

        if (dma) {
            /* Prepare the DMA transfer. */
            // TODO
            //ret = ata_channel_prepare_dma(device->channel, buf, count * device->block_size, write);
            //if (ret != STATUS_SUCCESS) {
            //    device_kprintf(device->disk.node, LOG_WARN, "failed to prepare DMA transfer (%d)\n", ret);
            //    return 0;
            //}
        }

        /* Start the transfer. */
        device->channel->ops->lba48_setup(device->channel, device->num, lba, count);
        ata_channel_command(device->channel, transfer_commands[write][true][dma]);
    } else {
        device_kprintf(device->disk.node, LOG_WARN, "attempted out of range transfer (%" PRIu64 ")\n", lba);
        return STATUS_DEVICE_ERROR;
    }

    *_transfer_count = count;
    *_dma            = dma;

    return STATUS_SUCCESS;
}

static status_t ata_device_io(ata_device_t *device, void *buf, uint64_t lba, size_t count, bool write) {
    status_t ret;

    ret = ata_channel_begin_command(device->channel, device->num);
    if (ret != STATUS_SUCCESS)
        return ret;

    while (count > 0) {
        size_t transfer_count;
        bool dma;
        ret = ata_device_begin_transfer(device, buf, lba, count, write, &transfer_count, &dma);
        if (ret != STATUS_SUCCESS)
            goto out;

        if (dma) {
            /* Start the DMA transfer and wait for it to finish. */
            // TODO
            //if (!ata_channel_perform_dma(device->channel)) {
            //    ata_channel_finish_dma(device->channel);
            //
            //    device_kprintf(device->disk.node, LOG_WARN, "timed out waiting for DMA transfer\n");
            //    ret = STATUS_DEVICE_ERROR;
            //    goto out;
            //}
            //
            //ret = ata_channel_finish_dma(device->channel);
            //
            //buf += transfer_count * device->disk.block_size;
        } else {
            /* Do a PIO transfer of each sector. */
            for (size_t i = 0; i < transfer_count; i++) {
                if (write) {
                    ret = ata_channel_write_pio(device->channel, buf, device->disk.block_size);
                } else {
                    ret = ata_channel_read_pio(device->channel, buf, device->disk.block_size);
                }

                if (ret != STATUS_SUCCESS)
                    break;

                buf += device->disk.block_size;
            }
        }

        if (ret != STATUS_SUCCESS) {
            uint8_t status = device->channel->ops->status(device->channel);
            uint8_t error  = device->channel->ops->error(device->channel);

            device_kprintf(
                device->disk.node, LOG_WARN,
                "%s of %zu block(s) at %" PRIu64 " failed (ret: %d, status: 0x%x, error: 0x%x)\n",
                (write) ? "write" : "read", transfer_count, lba, ret, status, error);

            goto out;
        }

        count -= transfer_count;
        lba   += transfer_count;
    }

    ret = STATUS_SUCCESS;

out:
    ata_channel_finish_command(device->channel);
    return ret;
}

static status_t ata_device_read_blocks(disk_device_t *_device, void *buf, uint64_t lba, size_t count) {
    ata_device_t *device = cast_ata_device(_device);

    return ata_device_io(device, buf, lba, count, false);
}

static status_t ata_device_write_blocks(disk_device_t *_device, const void *buf, uint64_t lba, size_t count) {
    ata_device_t *device = cast_ata_device(_device);

    return ata_device_io(device, (void *)buf, lba, count, true);
}

static disk_device_ops_t ata_device_ops = {
    .read_blocks  = ata_device_read_blocks,
    .write_blocks = ata_device_write_blocks,
};

static void copy_id_string(char *dest, char *src, size_t size) {
    for (size_t i = 0; i < size; i += 2)
        swap(src[i], src[i + 1]);

    /* Get rid of the trailing spaces. */
    size_t len;
    for (len = size; len > 0; len--) {
        if (src[len - 1] != ' ')
            break;
    }

    memcpy(dest, src, len);
    dest[len] = 0;
}

static inline uint16_t read_id16(const uint16_t *id, size_t word) {
    return le16_to_cpu(id[word]);
}

static inline uint32_t read_id32(const uint16_t *id, size_t word) {
    return le32_to_cpu(*(const uint32_t *)&id[word]);
}

static inline uint64_t read_id64(const uint16_t *id, size_t word) {
    return le64_to_cpu(*(const uint64_t *)&id[word]);
}

static bool process_id(ata_device_t *device, uint16_t *id) {
    device->version = 0;
    uint16_t major_version = read_id16(id, ATA_ID_MAJOR_VERSION);
    if (major_version != 0xffff) {
        for (device->version = 14; device->version >= 1; device->version--) {
            if (major_version & (1 << device->version))
                break;
        }
    }

    copy_id_string(device->model, (char *)&id[ATA_ID_MODEL], 40);
    copy_id_string(device->serial, (char *)&id[ATA_ID_SERIAL], 20);
    copy_id_string(device->revision, (char *)&id[ATA_ID_REVISION], 8);

    device_kprintf(
        device->disk.node, LOG_NOTICE, "ATA-%" PRIu16 " %s (revision: %s, serial: %s)\n",
        device->version, device->model, device->revision, device->serial);

    if (read_id16(id, ATA_ID_FEATURE_SET_2) & (1<<10))
        device->caps |= ATA_DEVICE_CAP_LBA48;

    device->disk.block_size          = 512;
    device->disk.physical_block_size = 512;

    /* This word is valid if bit 14 is set and bit 15 is clear. */
    uint16_t sector_size = read_id16(id, ATA_ID_SECTOR_SIZE);
    if ((sector_size & (3<<14)) == (1<<14)) {
        /* This bit indicates that logical sector size is more than 512 bytes. */
        if (sector_size & (1<<12))
            device->disk.block_size = read_id32(id, ATA_ID_LOGICAL_SECTOR_SIZE) * 2;

        /* Bits 3:0 indicate physical sector size in power of two logical
         * sectors. */
        uint32_t log_per_phys_shift = sector_size & 0xf;
        device->disk.physical_block_size = device->disk.block_size * (1<<log_per_phys_shift);
    }

    device_kprintf(
        device->disk.node, LOG_NOTICE, "block size: %" PRIu32 " bytes logical, %" PRIu32 " bytes physical\n",
        device->disk.block_size, device->disk.physical_block_size);

    if (device->caps & ATA_DEVICE_CAP_LBA48) {
        device->disk.block_count = read_id64(id, ATA_ID_LBA48_SECTOR_COUNT);
    } else {
        device->disk.block_count = read_id32(id, ATA_ID_SECTOR_COUNT);
    }

    device_kprintf(
        device->disk.node, LOG_NOTICE, "capacity: %" PRIu64 " MiB (blocks: %" PRIu64 ")\n",
        (device->disk.block_count * device->disk.block_size) / 1024 / 1024,
        device->disk.block_count);

    int mode = ATA_MODE_PIO;

    if (device->channel->caps & ATA_CHANNEL_CAP_DMA &&
        read_id16(id, ATA_ID_CAPABILITIES_1) & (1<<8))
    {
        int count = 0;

        #define MODE(_word, _bit, _mode) \
            if (read_id16(id, _word) & (1<<_bit)) { \
                mode = _mode; \
                count++; \
            }

        MODE(ATA_ID_MULTIWORD_DMA, 8,  ATA_MODE_MULTIWORD_0);
        MODE(ATA_ID_MULTIWORD_DMA, 9,  ATA_MODE_MULTIWORD_1);
        MODE(ATA_ID_MULTIWORD_DMA, 10, ATA_MODE_MULTIWORD_2);
        MODE(ATA_ID_ULTRA_DMA,     8,  ATA_MODE_UDMA_0);
        MODE(ATA_ID_ULTRA_DMA,     9,  ATA_MODE_UDMA_1);
        MODE(ATA_ID_ULTRA_DMA,     10, ATA_MODE_UDMA_2);
        MODE(ATA_ID_ULTRA_DMA,     11, ATA_MODE_UDMA_3);
        MODE(ATA_ID_ULTRA_DMA,     12, ATA_MODE_UDMA_4);
        MODE(ATA_ID_ULTRA_DMA,     13, ATA_MODE_UDMA_5);
        MODE(ATA_ID_ULTRA_DMA,     14, ATA_MODE_UDMA_6);

        /* Only one mode should be selected. */
        if (count > 1) {
            device_kprintf(
                device->disk.node, LOG_WARN, "device has more than one DMA mode selected, not using DMA\n");

            mode = ATA_MODE_PIO;
        } else if (count == 1) {
            device->caps |= ATA_DEVICE_CAP_DMA;
        }
    }

    if (mode == ATA_MODE_PIO && !(device->channel->caps & ATA_CHANNEL_CAP_PIO)) {
        device_kprintf(
            device->disk.node, LOG_ERROR, "skipping device without DMA on channel without PIO\n");

        return false;
    }

    device_kprintf(device->disk.node, LOG_NOTICE, "transfer mode: %s\n", ata_mode_strings[mode]);

    if (device->caps != 0) {
        device_kprintf(device->disk.node, LOG_NOTICE, "capabilities: ");

        if (device->caps & ATA_DEVICE_CAP_LBA48) kprintf(LOG_NOTICE, "LBA48 ");
        if (device->caps & ATA_DEVICE_CAP_DMA)   kprintf(LOG_NOTICE, "DMA ");

        kprintf(LOG_NOTICE, "\n");
    }

    return true;
}

/** Detect ATA device presence. */
void ata_device_detect(ata_channel_t *channel, uint8_t num) {
    status_t ret;

    if (!(channel->caps & ATA_CHANNEL_CAP_PIO)) {
        device_kprintf(channel->node, LOG_ERROR, "TODO: DMA identify\n");
        return;
    }

    uint16_t *id __cleanup_kfree = kmalloc(ATA_ID_COUNT * sizeof(uint16_t), MM_KERNEL);

    if (ata_channel_begin_command(channel, num) != STATUS_SUCCESS)
        return;

    /* Send an IDENTIFY DEVICE command.  */
    ata_channel_command(channel, ATA_CMD_IDENTIFY_DEVICE);

    // TODO: Packet device support.

    /* Perform a manual wait to see that either BSY or DRQ become set, which
     * indicates that the device is actually present. read_pio() will then wait
     * for BSY to be clear. This means we don't wait too long if the device is
     * not present (we'd otherwise hit the long read timeout). */
    ret = ata_channel_wait(
        channel, ATA_CHANNEL_WAIT_ANY | ATA_CHANNEL_WAIT_ERROR,
        ATA_STATUS_BSY | ATA_STATUS_DRQ, msecs_to_nsecs(25));

    if (ret == STATUS_SUCCESS) {
        /* Transfer the data. */
        ret = ata_channel_read_pio(channel, id, ATA_ID_COUNT * sizeof(uint16_t));
        if (ret != STATUS_SUCCESS) {
            device_kprintf(
                channel->node, LOG_WARN, "failed to read IDENTIFY response for device %" PRIu8 ": %d\n",
                num, ret);
        }
    }

    ata_channel_finish_command(channel);

    if (ret != STATUS_SUCCESS)
        return;

    /* Check whether we can use the device. */
    if (read_id16(id, ATA_ID_CONFIG) & (1<<15)) {
        device_kprintf(channel->node, LOG_WARN, "skipping non-ATA device %" PRIu8 "\n", num);
        return;
    } else if (!(read_id16(id, ATA_ID_CAPABILITIES_1) & (1<<9))) {
        device_kprintf(channel->node, LOG_WARN, "skipping non-LBA device %" PRIu8 "\n", num);
        return;
    }

    ata_device_t *device = kmalloc(sizeof(*device), MM_KERNEL | MM_ZERO);

    char name[4];
    snprintf(name, sizeof(name), "%" PRIu8, num);

    ret = disk_device_create_etc(&device->disk, name, channel->node);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(channel->node, LOG_ERROR, "failed to create device %" PRIu8 ": %d\n", num, ret);
        kfree(device);
        return;
    }

    device_add_kalloc(device->disk.node, device);

    device->disk.ops = &ata_device_ops;
    device->channel  = channel;
    device->num      = num;

    if (!process_id(device, id)) {
        disk_device_destroy(&device->disk);
        return;
    }

    ret = disk_device_publish(&device->disk);
    if (ret != STATUS_SUCCESS) {
        disk_device_destroy(&device->disk);
        return;
    }
}
