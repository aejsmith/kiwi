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

#include <assert.h>
#include <status.h>
#include <time.h>

#include "ata.h"

/**
 * Waits for device status to change according to the specified behaviour
 * flags.
 *
 * Note that when BSY is set in the status register, other bits must be ignored.
 * Therefore, if waiting for BSY, it must be the only bit specified to wait for
 * (unless ATA_CHANNEL_WAIT_ANY is set).
 *
 * There is also no need to wait for BSY to be cleared, as this is done
 * automatically.
 *
 * @param channel       Channel to wait on.
 * @param flags         Behaviour flags.
 * @param bits          Bits to wait for (used according to flags).
 * @param timeout       Timeout in microseconds.
 *
 * @return              Status code describing result of the operation.
 */
status_t ata_channel_wait(ata_channel_t *channel, uint32_t flags, uint8_t bits, nstime_t timeout) {
    assert(timeout > 0);

    uint8_t set   = (!(flags & ATA_CHANNEL_WAIT_CLEAR)) ? bits : 0;
    uint8_t clear = (flags & ATA_CHANNEL_WAIT_CLEAR) ? bits : 0;
    bool any      = flags & ATA_CHANNEL_WAIT_ANY;
    bool error    = flags & ATA_CHANNEL_WAIT_ERROR;

    /* If waiting for BSY, ensure no other bits are set. Otherwise, add BSY
     * to the bits to wait to be clear. */
    if (set & ATA_STATUS_BSY) {
        assert(any || (set == ATA_STATUS_BSY && clear == 0));
    } else {
        clear |= ATA_STATUS_BSY;
    }

    nstime_t elapsed = 0;
    while (timeout) {
        uint8_t status = channel->ops->status(channel);

        if (error) {
            if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_ERR || status & ATA_STATUS_DF))
                return STATUS_DEVICE_ERROR;
        }

        if (!(status & clear) && ((any && (status & set)) || (status & set) == set))
            return STATUS_SUCCESS;

        nstime_t step;
        if (elapsed < 1000) {
            step = min(timeout, 10);
            spin(step);
        } else {
            step = min(timeout, 1000);
            delay(step);
        }

        timeout -= step;
        elapsed += step;
    }

    return STATUS_TIMED_OUT;
}

/**
 * Handles an interrupt indicating completion of DMA on an ATA channel. The
 * calling driver should ensure that the interrupt came from the channel before
 * calling this function.
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
    status_t ret;

    /* Reset the channel to a good state. */
    ret = channel->ops->reset(channel);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(channel->node, LOG_ERROR, "failed to reset device: %d\n", ret);
        return ret;
    }

    // TODO: Is reset status checking necessary per-device? SRST bit applies to
    // both but status is per-device.

    // TODO: Probe devices
    return STATUS_SUCCESS;
}
