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

/** Copies an ATA identification string (modifies source string). */
static void copy_ident_string(char *dest, char *src, size_t size) {
    char *ptr = src;
    for (size_t i = 0; i < size; i += 2) {
        char tmp = *ptr;
        *ptr = *(ptr + 1);
        ptr++;
        *ptr = tmp;
        ptr++;
    }

    /* Get rid of the trailing spaces. */
    size_t len;
    for (len = size; len > 0; len--) {
        if (src[len - 1] != ' ')
            break;
    }

    memcpy(dest, src, len);
    dest[len] = 0;
}

/** Detect ATA device presence. */
void ata_device_detect(ata_channel_t *channel, uint8_t num) {
    status_t ret;

    if (!(channel->caps & ATA_CHANNEL_CAP_PIO)) {
        device_kprintf(channel->node, LOG_ERROR, "TODO: DMA identify\n");
        return;
    }

    uint16_t *ident __cleanup_kfree = kmalloc(512, MM_KERNEL);

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
        ret = ata_channel_read_pio(channel, ident, 512);
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
    if (le16_to_cpu(ident[0]) & (1<<15)) {
        device_kprintf(channel->node, LOG_WARN, "skipping non-ATA device %" PRIu8 "\n", num);
        return;
    } else if (!(le16_to_cpu(ident[49]) & (1<<9))) {
        device_kprintf(channel->node, LOG_WARN, "skipping non-LBA device %" PRIu8 "\n", num);
        return;
    }

    // TODO: Destruction: Device needs to get freed - add way to attach this
    // allocation to the device.
    ata_device_t *device = kmalloc(sizeof(*device), MM_KERNEL | MM_ZERO);

    copy_ident_string(device->model, (char *)(ident + 27), 40);
    copy_ident_string(device->serial, (char *)(ident + 10), 20);
    copy_ident_string(device->revision, (char *)(ident + 23), 8);

    // TODO: Use device node
    device_kprintf(channel->node, LOG_NOTICE, "device %u\n", num);
    device_kprintf(channel->node, LOG_NOTICE, "model:    %s\n", device->model);
    device_kprintf(channel->node, LOG_NOTICE, "serial:   %s\n", device->serial);
    device_kprintf(channel->node, LOG_NOTICE, "revision: %s\n", device->revision);
}
