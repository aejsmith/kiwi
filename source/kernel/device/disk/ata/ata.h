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
 * @brief               ATA internal definitions.
 */

#pragma once

#include <device/disk/ata.h>
#include <device/disk/disk.h>

enum {
    ATA_CHANNEL_WAIT_SET    = 0,        /**< Wait for specified bits to be set (default). */
    ATA_CHANNEL_WAIT_CLEAR  = (1<<0),   /**< Wait for specified bits to be clear. */
    ATA_CHANNEL_WAIT_ANY    = (1<<1),   /**< Wait for any specified bit to be set. */
    ATA_CHANNEL_WAIT_ERROR  = (1<<2),   /**< Check for and report errors. */
};

extern status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num);
extern void ata_channel_finish_command(ata_channel_t *channel);
extern void ata_channel_command(ata_channel_t *channel, uint8_t cmd);
extern status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count);
extern status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count);
extern status_t ata_channel_wait(ata_channel_t *channel, uint32_t flags, uint8_t bits, nstime_t timeout);

extern status_t ata_channel_create_etc(
    module_t *module, ata_channel_t *channel, const char *name,
    device_t *parent);

/** ATA device structure. */
typedef struct ata_device {
    disk_device_t disk;

    /** Information from IDENTIFY. */
    char model[40 + 1];
    char serial[20 + 1];
    char revision[8 + 1];
    uint32_t caps;                      /**< Device capabilities (ATA_DEVICE_CAP_*). */
} ata_device_t;

DEFINE_CLASS_CAST(ata_device, disk_device, disk);

/** ATA device capabilities. */
enum {
    ATA_DEVICE_CAP_LBA48    = (1<<0),   /**< Device supports 48-bit addressing. */
};

extern void ata_device_detect(ata_channel_t *channel, uint8_t num);
