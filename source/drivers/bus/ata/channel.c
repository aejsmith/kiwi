/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		ATA channel management.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "ata_priv.h"

/** Next channel ID. */
static atomic_t next_channel_id = 0;

/** Read from a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
uint8_t ata_channel_read_ctrl(ata_channel_t *channel, int reg) {
	assert(channel->ops->read_ctrl);
	return channel->ops->read_ctrl(channel, reg);
}

/** Write to a control register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
void ata_channel_write_ctrl(ata_channel_t *channel, int reg, uint8_t val) {
	assert(channel->ops->write_ctrl);
	channel->ops->write_ctrl(channel, reg, val);
}

/** Read from a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to read from.
 * @return		Value read. */
uint8_t ata_channel_read_cmd(ata_channel_t *channel, int reg) {
	assert(channel->ops->read_cmd);
	return channel->ops->read_cmd(channel, reg);
}

/** Write to a command register.
 * @param channel	Channel to read from.
 * @param reg		Register to write to.
 * @param val		Value to write. */
void ata_channel_write_cmd(ata_channel_t *channel, int reg, uint8_t val) {
	assert(channel->ops->write_cmd);
	channel->ops->write_cmd(channel, reg, val);
}

/** Wait for DRQ and perform a PIO data read.
 * @param channel	Channel to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_read_pio(ata_channel_t *channel, void *buf, size_t count) {
	status_t ret;

	assert(channel->ops->read_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->read_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Wait for DRQ and perform a PIO data write.
 * @param channel	Channel to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @return		STATUS_SUCCESS if succeeded, STATUS_DEVICE_ERROR if a
 *			device error occurred, or STATUS_TIMED_OUT if timed out
 *			while waiting for DRQ. */
status_t ata_channel_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
	status_t ret;

	assert(channel->ops->write_pio);

	/* Wait for DRQ to be set and BSY to be clear. */
	ret = ata_channel_wait(channel, ATA_STATUS_DRQ, 0, false, true, SECS2USECS(5));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	channel->ops->write_pio(channel, buf, count);
	return STATUS_SUCCESS;
}

/** Get the content of the alternate status register.
 * @param channel	Channel to get from.
 * @return		Value of alternate status register. */
uint8_t ata_channel_status(ata_channel_t *channel) {
	return ata_channel_read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

/** Get the content of the error register.
 * @param channel	Channel to get from.
 * @return		Value of error register. */
uint8_t ata_channel_error(ata_channel_t *channel) {
	return ata_channel_read_cmd(channel, ATA_CMD_REG_ERR);
}

/** Get the currently selected device.
 * @param channel	Channel to get from.
 * @return		Current selected device. */
uint8_t ata_channel_selected(ata_channel_t *channel) {
	return (ata_channel_read_cmd(channel, ATA_CMD_REG_DEVICE) >> 4) & (1<<0);
}

/** Issue a command to the selected device.
 * @param channel	Channel to perform command on.
 * @param cmd		Command to perform. */
void ata_channel_command(ata_channel_t *channel, uint8_t cmd) {
	ata_channel_write_cmd(channel, ATA_CMD_REG_CMD, cmd);
	spin(1);
}

/** Trigger a software reset of both devices.
 * @param channel	Channel to reset. */
void ata_channel_reset(ata_channel_t *channel) {
	/* See 11.2 - Software reset protocol (in Volume 2). We wait for longer
	 * than necessary to be sure it's done. */
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_SRST);
	usleep(20);
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, 0);
	usleep(MSECS2USECS(150));
	ata_channel_wait(channel, 0, 0, false, false, 1000);

	/* Disable interrupts. */
	ata_channel_write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_NIEN);
}

/** Wait for device status to change.
 * @note		When BSY is set in the status register, other bits must
 *			be ignored. Therefore, if waiting for BSY, it must be
 *			the only bit specified to wait for (unless any is true).
 *			There is also no need to wait for BSY to be cleared, as
 *			this is done automatically.
 * @param channel	Channel to wait on.
 * @param set		Bits to wait to be set.
 * @param clear		Bits to wait to be clear.
 * @param any		Wait for any bit in set to be set.
 * @param error		Check for errors/faults.
 * @param timeout	Timeout in microseconds.
 * @return		Status code describing result of the operation. */
status_t ata_channel_wait(ata_channel_t *channel, uint8_t set, uint8_t clear, bool any,
                          bool error, useconds_t timeout) {
	uint8_t status;
	useconds_t i;

	assert(timeout);

	/* If waiting for BSY, ensure no other bits are set. Otherwise, add BSY
	 * to the bits to wait to be clear. */
	if(set & ATA_STATUS_BSY) {
		assert(any || (set == ATA_STATUS_BSY && clear == 0));
	} else {
		clear |= ATA_STATUS_BSY;
	}

	while(timeout) {
		status = ata_channel_status(channel);
		if(error) {
			if(!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_ERR || status & ATA_STATUS_DF)) {
				return STATUS_DEVICE_ERROR;
			}
		}
		if(!(status & clear) && ((any && (status & set)) || (status & set) == set)) {
			return STATUS_SUCCESS;
		}

		i = (timeout < 1000) ? timeout : 1000;
		usleep(i);
		timeout -= i;
	}

	return STATUS_TIMED_OUT;
}

/** Prepares to perform a command on a channel.
 *
 * Locks the channel, waits for it to become ready (DRQ and BSY set to 0),
 * selects the specified device and waits for it to become ready again. This
 * implements the HI1:Check_Status and HI2:Device_Select parts of the Bus idle
 * protocol. It should be called prior to performing any command. When the
 * command is finished, ata_channel_finish_command() must be called.
 *
 * @param channel	Channel to perform command on.
 * @param num		Device number to select (0 or 1).
 *
 * @return		Status code describing the result of the operation.
 */
status_t ata_channel_begin_command(ata_channel_t *channel, uint8_t num) {
	bool attempted = false;

	assert(num == 0 || num == 1);

	/* Begin by locking the channel, to prevent other devices on it from
	 * interfering with our operation. */
	mutex_lock(&channel->lock);

	while(true) {
		/* Wait for BSY and DRQ to be cleared (BSY is checked automatically). */
		if(ata_channel_wait(channel, 0, ATA_STATUS_DRQ, false, false, SECS2USECS(5)) != STATUS_SUCCESS) {
			kprintf(LOG_WARN, "ata: timed out while waiting for channel %d to become idle (status: 0x%x)\n",
			        channel->id, ata_channel_status(channel));
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Check whether the device is selected. */
		if(ata_channel_selected(channel) == num) {
			return STATUS_SUCCESS;
		}

		/* Fail if we've already attempted to set the device. */
		if(attempted) {
			kprintf(LOG_WARN, "ata: channel %d did not respond to setting device %u\n",
			        channel->id, num);
			mutex_unlock(&channel->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* Try to set it and then wait again. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, num << 4);
		spin(1);
	}
}

/** Releases the channel after a command.
 * @param channel	Channel to finish on. */
void ata_channel_finish_command(ata_channel_t *channel) {
	mutex_unlock(&channel->lock);
}

/** Register a new ATA channel.
 * @param parent	Parent in the device tree.
 * @param ops		Channel operations structure.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to channel structure if added, NULL if not. */
ata_channel_t *ata_channel_add(device_t *parent, ata_channel_ops_t *ops, void *data) {
	device_attr_t attr[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "ata-channel" } },
	};
	char name[DEVICE_NAME_MAX];
	ata_channel_t *channel;
	status_t ret;

	assert(parent);
	assert(ops);

	/* Create a new channel structure. */
	channel = kmalloc(sizeof(*channel), MM_SLEEP);
	mutex_init(&channel->lock, "ata_channel_lock", 0);
	spinlock_init(&channel->irq_lock, "ata_channel_irq_lock");
	condvar_init(&channel->irq_cv, "ata_channel_irq_cv");
	channel->ops = ops;
	channel->data = data;

	/* Check presence by writing a value to the low LBA port on the channel,
	 * then reading it back. If the value is the same, it is present. */
	ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, 0xAB);
	if(ata_channel_read_cmd(channel, ATA_CMD_REG_LBA_LOW) != 0xAB) {
		kfree(channel);
		return NULL;
	}

	/* Allocate an ID for the controller. */
	channel->id = atomic_inc(&next_channel_id);

	/* Publish it in the device tree. */
	sprintf(name, "ata%d", channel->id);
	ret = device_create(name, parent, NULL, NULL, attr, ARRAYSZ(attr), &channel->node);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ata: could not create device tree node for channel %d (%d)\n",
			channel->id, ret);
		kfree(channel);
		return NULL;
	}

	/* Reset the channel to a decent state. */
	ata_channel_reset(channel);
	return channel;
}
MODULE_EXPORT(ata_channel_add);

/** Scan an ATA channel for devices.
 * @param channel	Channel to scan. */
void ata_channel_scan(ata_channel_t *channel) {
	ata_device_detect(channel, 0);
	ata_device_detect(channel, 1);
}
MODULE_EXPORT(ata_channel_scan);
