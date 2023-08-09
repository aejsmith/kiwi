/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * Prepares to perform a command on a channel. This locks the channel, waits
 * for it to become ready (DRQ and BSY set to 0), selects the specified device
 * and waits for it to become ready again.
 *
 * This implements the HI1:Check_Status and HI2:Device_Select parts of the Bus
 * idle protocol. It should be called prior to performing any command. When the
 * command is finished, ata_channel_finish_command() must be called to unlock
 * the channel.
 *
 * @param channel       Channel to perform command on.
 * @param num           Device number to select (0 or 1).
 *
 * @return              Status code describing the result of the operation.
 */
status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num) {
    assert(channel->device_mask & (1<<num));

    mutex_lock(&channel->command_lock);

    /* Clear any pending interrupts. */
    while (semaphore_down_etc(&channel->irq_sem, 0, 0) == STATUS_SUCCESS)
        ;

    bool attempted = false;
    while (true) {
        /* Wait for BSY and DRQ to be cleared (BSY is checked automatically). */
        status_t ret = ata_channel_wait(channel, ATA_CHANNEL_WAIT_CLEAR, ATA_STATUS_DRQ, secs_to_nsecs(5));
        if (ret != STATUS_SUCCESS) {
            device_kprintf(
                channel->node, LOG_WARN,
                "timed out while waiting for channel to become idle (status: 0x%x)\n",
                channel->ops->status(channel));

            mutex_unlock(&channel->command_lock);
            return STATUS_DEVICE_ERROR;
        }

        /* Check whether the required device is selected. */
        if (channel->ops->selected(channel) == num)
            return STATUS_SUCCESS;

        /* Fail if we've already attempted to set the device. */
        if (attempted) {
            device_kprintf(channel->node, LOG_WARN, "channel did not respond to setting device %u\n", num);

            mutex_unlock(&channel->command_lock);
            return STATUS_DEVICE_ERROR;
        }

        attempted = true;

        /* Try to set it and then wait again. */
        channel->ops->select(channel, num);
    }
}

/** Releases the channel after a command.
 * @param channel       Channel to finish on. */
void ata_channel_finish_command(ata_channel_t *channel) {
    mutex_unlock(&channel->command_lock);
}

/**
 * Issue a command to the selected device. This must be performed within a
 * ata_channel_begin_command()/ata_channel_finish_command() pair.
 *
 * @param channel       Channel to perform command on.
 * @param cmd           Command to perform.
 */
void ata_channel_command(ata_channel_t *channel, uint8_t cmd) {
    assert(mutex_held(&channel->command_lock));

    channel->ops->command(channel, cmd);

    /* Command protocols all say to wait 400ns before checking status, this is
     * the time the device must set BSY within. */
    spin(400);
}

/** Waits for DRQ and perform a PIO data read.
 * @param channel       Channel to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @return              STATUS_SUCCESS if succeeded.
 *                      STATUS_DEVICE_ERROR if a device error occurred.
 *                      STATUS_TIMED_OUT if timed out while waiting for DRQ. */
status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count) {
    assert(channel->caps & ATA_CHANNEL_CAP_PIO);

    /* Wait for DRQ to be set and BSY to be clear. */
    status_t ret = ata_channel_wait(channel, ATA_CHANNEL_WAIT_ERROR, ATA_STATUS_DRQ, secs_to_nsecs(5));
    if (ret != STATUS_SUCCESS)
        return ret;

    channel->ops->read_pio(channel, buf, count);
    return STATUS_SUCCESS;
}

/** Waits for DRQ and perform a PIO data write.
 * @param channel       Channel to write to.
 * @param buf           Buffer to write from.
 * @param count         Number of bytes to write.
 * @return              STATUS_SUCCESS if succeeded.
 *                      STATUS_DEVICE_ERROR if a device error occurred.
 *                      STATUS_TIMED_OUT if timed out while waiting for DRQ. */
status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
    assert(channel->caps & ATA_CHANNEL_CAP_PIO);

    /* Wait for DRQ to be set and BSY to be clear. */
    status_t ret = ata_channel_wait(channel, ATA_CHANNEL_WAIT_ERROR, ATA_STATUS_DRQ, secs_to_nsecs(5));
    if (ret != STATUS_SUCCESS)
        return ret;

    channel->ops->write_pio(channel, buf, count);
    return STATUS_SUCCESS;
}

/**
 * Starts a DMA transfer and waits for it to complete. The caller needs to have
 * called channel->ops->prepare_dma() prior to this to set up the transfer. This
 * will handle everything else, including timeout and finishing the transfer.
 *
 * @param channel       Channel to perform transfer on.
 *
 * @return              STATUS_SUCCESS if succeeded.
 *                      STATUS_DEVICE_ERROR if a device error occurred.
 *                      STATUS_TIMED_OUT if timed out while waiting for IRQ.
 */
status_t ata_channel_perform_dma(ata_channel_t *channel) {
    assert(channel->caps & ATA_CHANNEL_CAP_DMA);

    channel->ops->start_dma(channel);
    status_t wait_ret   = semaphore_down_etc(&channel->irq_sem, secs_to_nsecs(5), 0);
    status_t finish_ret = channel->ops->finish_dma(channel);

    return (wait_ret == STATUS_SUCCESS) ? finish_ret : wait_ret;
}

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
        if (elapsed < msecs_to_nsecs(1)) {
            step = min(timeout, usecs_to_nsecs(10));
            spin(step);
        } else {
            step = min(timeout, msecs_to_nsecs(1));
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
 * calling this function. This is safe to call from interrupt context.
 *
 * @param channel       Channel that the interrupt occurred on.
 */
__export void ata_channel_irq(ata_channel_t *channel) {
    /* Ignore interrupts if there's no pending command, though we should not
     * really get an interrupt left over from a previous command, as cancelling
     * a DMA transfer (finish_dma) should ensure we don't get a stale interrupt
     * after that. */
    if (mutex_held(&channel->command_lock)) {
        semaphore_up(&channel->irq_sem, 1);
    } else {
        device_kprintf(channel->node, LOG_WARN, "received unexpected interrupt");
    }
}

status_t ata_channel_create_etc(module_t *module, ata_channel_t *channel, const char *name, device_t *parent) {
    memset(channel, 0, sizeof(*channel));

    mutex_init(&channel->command_lock, "ata_command_lock", 0);
    semaphore_init(&channel->irq_sem, "ata_irq_sem", 0);

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

    /* Check device presence. */
    channel->device_mask = 0;
    if (channel->ops->present(channel, 0))
        channel->device_mask |= (1<<0);
    if (channel->caps & ATA_CHANNEL_CAP_SLAVE && channel->ops->present(channel, 1))
        channel->device_mask |= (1<<1);

    /* Reset the channel to a good state. */
    ret = channel->ops->reset(channel);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(channel->node, LOG_ERROR, "failed to reset device: %d\n", ret);
        return ret;
    }

    device_publish(channel->node);

    /* Probe devices. */
    if (channel->device_mask & (1<<0))
        ata_device_detect(channel, 0);
    if (channel->device_mask & (1<<1))
        ata_device_detect(channel, 1);

    return STATUS_SUCCESS;
}
