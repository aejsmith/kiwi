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

#include <status.h>
#include <time.h>

#include "ata.h"

static status_t ata_sff_channel_reset(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    /* See 11.2 - Software reset protocol (in Volume 2). We wait for longer
     * than necessary to be sure it's done. */
    channel->ops->write_ctrl(channel, ATA_CTRL_REG_DEV_CTRL, ATA_DEV_CTRL_SRST | ATA_DEV_CTRL_NIEN);
    delay(usecs_to_nsecs(20));
    channel->ops->write_ctrl(channel, ATA_CTRL_REG_DEV_CTRL, ATA_DEV_CTRL_NIEN);
    delay(msecs_to_nsecs(20));

    /* Wait for BSY to clear. */
    ata_channel_wait(&channel->ata, 0, 0, 1000);

    /* Clear any pending interrupts. */
    channel->ops->read_cmd(channel, ATA_CMD_REG_STATUS);
    return STATUS_SUCCESS;
}

static uint8_t ata_sff_channel_status(ata_channel_t *_channel) {
    ata_sff_channel_t *channel = cast_ata_sff_channel(_channel);

    return channel->ops->read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

static ata_channel_ops_t ata_sff_channel_ops = {
    .reset  = ata_sff_channel_reset,
    .status = ata_sff_channel_status,
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
