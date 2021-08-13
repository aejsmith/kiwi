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

#include "ata.h"

static ata_channel_ops_t ata_sff_channel_ops = {

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
