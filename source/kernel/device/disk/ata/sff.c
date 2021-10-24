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
 * @brief               SFF-style ATA channel implementation.
 *
 * Small Form Factor (SFF) is the legacy-style IDE interface. These functions
 * add an extra layer on top of the base ATA channel interface to handle parts
 * of this interface common to all drivers for SFF controllers.
 */

#include <lib/string.h>

#include <assert.h>
#include <status.h>
#include <time.h>

#include "ata.h"

/** Flush writes to the channel registers. */
static void ata_sff_channel_flush(ata_sff_channel_t *channel) {
    channel->ops->read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

static uint8_t ata_sff_channel_status(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);
    return channel->ops->read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

static uint8_t ata_sff_channel_error(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);
    return channel->ops->read_cmd(channel, ATA_CMD_REG_ERROR);
}

static uint8_t ata_sff_channel_selected(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);
    return (channel->ops->read_cmd(channel, ATA_CMD_REG_DEVICE) >> 4) & 1;
}

static void ata_sff_channel_select(ata_channel_t *_channel, uint8_t num) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    assert(num < 2);

    channel->ops->write_cmd(channel, ATA_CMD_REG_DEVICE, num << 4);

    /* Flush by checking status and then wait after selecting. */
    ata_sff_channel_flush(channel);
    spin(400);
}

static status_t ata_sff_channel_reset(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    ata_sff_channel_select(&channel->ata, 0);

    /* See 11.2 - Software reset protocol (in Volume 2). We wait for longer
     * than necessary to be sure it's done. */
    channel->ops->write_ctrl(channel, ATA_CTRL_REG_DEV_CTRL, ATA_DEV_CTRL_SRST | ATA_DEV_CTRL_NIEN);
    delay(usecs_to_nsecs(20));
    channel->ops->write_ctrl(channel, ATA_CTRL_REG_DEV_CTRL, ATA_DEV_CTRL_NIEN);
    delay(msecs_to_nsecs(20));

    /* Wait for BSY to clear and clear any pending interrupts. */
    ata_channel_wait(&channel->ata, 0, 0, 1000);
    channel->ops->read_cmd(channel, ATA_CMD_REG_STATUS);

    if (channel->ata.device_mask & (1<<1)) {
        /* Do the same for slave. */
        ata_channel_wait(&channel->ata, 0, 0, 1000);
        channel->ops->read_cmd(channel, ATA_CMD_REG_STATUS);
    }

    return STATUS_SUCCESS;
}

static bool ata_sff_channel_present(ata_channel_t *_channel, uint8_t num) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    assert(num < 2);

    ata_sff_channel_select(&channel->ata, num);

    if (ata_sff_channel_selected(&channel->ata) != num)
        return false;

    /* Check presence by writing a pattern to some registers and see if we can
     * get the same pattern back. Procedure borrowed from Linux. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, 0x55);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, 0xaa);

    channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, 0xaa);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, 0x55);

    channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, 0x55);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, 0xaa);

    uint8_t sector_count = channel->ops->read_cmd(channel, ATA_CMD_REG_SECTOR_COUNT);
    uint8_t lba_low      = channel->ops->read_cmd(channel, ATA_CMD_REG_LBA_LOW);

    return sector_count == 0x55 && lba_low == 0xaa;
}

static void ata_sff_channel_command(ata_channel_t *_channel, uint8_t cmd) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    channel->ops->write_cmd(channel, ATA_CMD_REG_CMD, cmd);
    ata_sff_channel_flush(channel);
}

static void ata_sff_channel_lba28_setup(ata_channel_t *_channel, uint8_t device, uint64_t lba, size_t count) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    /* Send a NULL to the feature register. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_FEATURES, 0);

    /* Write out the number of blocks to read. 0 means 256. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, (count == 256) ? 0 : count);

    /* Specify the address of the block. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW,  lba & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_MID,  (lba >> 8) & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

    /* Device number with LBA bit set, and last 4 bits of address. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device << 4) | ((lba >> 24) & 0xf));
}

static void ata_sff_channel_lba48_setup(ata_channel_t *_channel, uint8_t device, uint64_t lba, size_t count) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    /* Send 2 NULLs to the feature register. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_FEATURES, 0);
    channel->ops->write_cmd(channel, ATA_CMD_REG_FEATURES, 0);

    /* Write out the number of blocks to read. */
    if (count == 65536) {
        channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, 0);
        channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, 0);
    } else {
        channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, (count >> 8) & 0xff);
        channel->ops->write_cmd(channel, ATA_CMD_REG_SECTOR_COUNT, count & 0xff);
    }

    /* Specify the address of the block. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW,  (lba >> 24) & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_LOW,  lba & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_MID,  (lba >> 32) & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_MID,  (lba >> 8) & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 40) & 0xff);
    channel->ops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

    /* Device number with LBA bit set. */
    channel->ops->write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device << 4));
}

static void ata_sff_channel_read_pio(ata_channel_t *_channel, void *buf, size_t count) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);
    channel->ops->read_pio(channel, buf, count);
}

static void ata_sff_channel_write_pio(ata_channel_t *_channel, const void *buf, size_t count) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);
    channel->ops->write_pio(channel, buf, count);
}

static const ata_channel_ops_t ata_sff_channel_ops = {
    .reset       = ata_sff_channel_reset,
    .status      = ata_sff_channel_status,
    .error       = ata_sff_channel_error,
    .selected    = ata_sff_channel_selected,
    .select      = ata_sff_channel_select,
    .present     = ata_sff_channel_present,
    .command     = ata_sff_channel_command,
    .lba28_setup = ata_sff_channel_lba28_setup,
    .lba48_setup = ata_sff_channel_lba48_setup,
    .read_pio    = ata_sff_channel_read_pio,
    .write_pio   = ata_sff_channel_write_pio,
};

/** Initializes a new SFF-style ATA channel.
 * @see                 ata_channel_create(). */
__export status_t ata_sff_channel_create(ata_sff_channel_t *channel, const char *name, device_t *parent) {
    memset(channel, 0, sizeof(*channel));

    status_t ret = ata_channel_create_etc(module_caller(), &channel->ata, name, parent);
    if (ret != STATUS_SUCCESS)
        return ret;

    channel->ata.ops = &ata_sff_channel_ops;

    return STATUS_SUCCESS;
}
