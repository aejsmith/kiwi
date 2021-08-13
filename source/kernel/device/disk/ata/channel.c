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
 * @brief               ATA channel implementation.
 */

#include <lib/string.h>

#include <status.h>

#include "ata.h"

/**
 * Handles an interrupt on an ATA channel. The calling driver should ensure that
 * the interrupt came from the channel before calling this function.
 *
 * @param channel       Channel that the interrupt occurred on.
 */
__export void ata_channel_interrupt(ata_channel_t *channel) {
    // TODO: Ignore interrupts before the channel is published.
    // TODO: If this ends up safe for interrupt context, get rid of threaded
    // handler in pci_ata.
}

status_t ata_channel_create_etc(module_t *module, ata_channel_t *channel, const char *name, device_t *parent) {
    memset(channel, 0, sizeof(*channel));

    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = "ata_channel" } },
    };

    // TODO: Ops... destroy
    return device_create_etc(
        module, name, parent, NULL, channel, attrs, array_size(attrs),
        &channel->node);
}

/**
 * Initializes a new ATA channel. This only creates a device tree node and
 * initializes some state in the device. Once the driver has completed
 * initialization, it should call ata_channel_publish().
 *
 * @param channel       Channel to initialize.
 * @param name          Name for the device.
 * @param parent        Parent device node (e.g. controller device).
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t ata_channel_create(ata_channel_t *channel, const char *name, device_t *parent) {
    return ata_channel_create_etc(module_caller(), channel, name, parent);
}

/**
 * Publishes an ATA channel. This completes initialization after the driver
 * has finished initialization, scans the channel for devices, and publishes
 * it for use.
 *
 * @param channel       Channel to publish.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t ata_channel_publish(ata_channel_t *channel) {
    // TODO
    return STATUS_SUCCESS;
}
