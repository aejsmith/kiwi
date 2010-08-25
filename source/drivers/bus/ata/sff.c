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
 * @brief		SFF ATA operations.
 */

#include <lib/string.h>

#include <assert.h>
#include <status.h>
#include <module.h>
#include <time.h>

#include "ata_priv.h"

/** Reset the channel.
 * @param channel	Channel to reset.
 * @return		Status code describing result of operation. */
static status_t ata_sff_reset(ata_channel_t *channel) {
	/* See 11.2 - Software reset protocol (in Volume 2). We wait for longer
	 * than necessary to be sure it's done. */
	channel->sops->write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_SRST | ATA_DEVCTRL_NIEN);
	usleep(20);
	channel->sops->write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_NIEN);
	usleep(MSECS2USECS(150));
	ata_channel_wait(channel, 0, 0, false, false, 1000);

	/* Clear any pending interrupts. */
	channel->sops->read_cmd(channel, ATA_CMD_REG_STATUS);
	return STATUS_SUCCESS;
}

/** Get the content of the status register.
 * @param channel	Channel to get status from.
 * @return		Content of the status register. */
static uint8_t ata_sff_status(ata_channel_t *channel) {
	return channel->sops->read_ctrl(channel, ATA_CTRL_REG_ALT_STATUS);
}

/** Get the content of the error register.
 * @param channel	Channel to get error from.
 * @return		Content of the error register. */
static uint8_t ata_sff_error(ata_channel_t *channel) {
	return channel->sops->read_cmd(channel, ATA_CMD_REG_ERR);
}

/** Get the selected device on a channel.
 * @param channel	Channel to get selected device from.
 * @return		Currently selected device number. */
static uint8_t ata_sff_selected(ata_channel_t *channel) {
	return (channel->sops->read_cmd(channel, ATA_CMD_REG_DEVICE) >> 4) & (1<<0);
}

/** Change the selected device on a channel.
 * @param channel	Channel to select on.
 * @param num		Device number to select.
 * @return		Whether the requested device is present. */
static bool ata_sff_select(ata_channel_t *channel, uint8_t num) {
	channel->sops->write_cmd(channel, ATA_CMD_REG_DEVICE, num << 4);
	return true;
}

/** Execute a command.
 * @param channel	Channel to execute on.
 * @param cmd		Command to execute. */
static void ata_sff_command(ata_channel_t *channel, uint8_t cmd) {
	channel->sops->write_cmd(channel, ATA_CMD_REG_CMD, cmd);
}

/** Set up registers for an LBA28 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
static void ata_sff_lba28_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	/* Send a NULL to the feature register. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_FEAT, 0);

	/* Write out the number of blocks to read. 0 means 256. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_COUNT, (count == 256) ? 0 : count);

	/* Specify the address of the block. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

	/* Device number with LBA bit set, and last 4 bits of address. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device << 4) | ((lba >> 24) & 0xf));
}

/** Set up registers for an LBA48 transfer.
 * @param channel	Channel to set up on.
 * @param device	Device number to operate on.
 * @param lba		LBA to transfer from/to.
 * @param count		Sector count. */
static void ata_sff_lba48_setup(ata_channel_t *channel, uint8_t device, uint64_t lba, size_t count) {
	/* Send 2 NULLs to the feature register. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_FEAT, 0);
	channel->sops->write_cmd(channel, ATA_CMD_REG_FEAT, 0);

	/* Write out the number of blocks to read. */
	if(count == 65536) {
		channel->sops->write_cmd(channel, ATA_CMD_REG_COUNT, 0);
		channel->sops->write_cmd(channel, ATA_CMD_REG_COUNT, 0);
	} else {
		channel->sops->write_cmd(channel, ATA_CMD_REG_COUNT, (count >> 8) & 0xff);
		channel->sops->write_cmd(channel, ATA_CMD_REG_COUNT, count & 0xff);
	}

	/* Specify the address of the block. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, (lba >> 24) & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 32) & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 40) & 0xff);
	channel->sops->write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

	/* Device number with LBA bit set. */
	channel->sops->write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device << 4));
}


/** Perform a PIO data read.
 * @param channel	Channel to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read. */
static void ata_sff_read_pio(ata_channel_t *channel, void *buf, size_t count) {
	assert(channel->sops->read_pio);
	channel->sops->read_pio(channel, buf, count);
}

/** Perform a PIO data write.
 * @param channel	Channel to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write. */
static void ata_sff_write_pio(ata_channel_t *channel, const void *buf, size_t count) {
	assert(channel->sops->write_pio);
	channel->sops->write_pio(channel, buf, count);
}

/** Prepare a DMA transfer.
 * @param channel	Channel to perform on.
 * @param vec		Array of block descriptions. Each block will
 *			cover no more than 1 page. The contents of this
 *			array are guaranteed to conform to the
 *			constraints specified to ata_channel_add().
 * @param count		Number of array entries.
 * @param write		Whether the transfer is a write.
 * @return		Status code describing result of operation. */
static status_t ata_sff_prepare_dma(ata_channel_t *channel, const ata_dma_transfer_t *vec,
	                            size_t count, bool write) {
	assert(channel->sops->prepare_dma);
	return channel->sops->prepare_dma(channel, vec, count, write);
}

/** Start a DMA transfer.
 * @param channel	Channel to start on. */
static void ata_sff_start_dma(ata_channel_t *channel) {
	/* Enable interrupts. */
	channel->sops->write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, 0);

	assert(channel->sops->start_dma);
	channel->sops->start_dma(channel);
}

/** Clean up after a DMA transfer.
 * @param channel	Channel to clean up on.
 * @return		Status code describing result of the transfer. */
static status_t ata_sff_finish_dma(ata_channel_t *channel) {
	/* Disable interrupts. */
	channel->sops->write_ctrl(channel, ATA_CTRL_REG_DEVCTRL, ATA_DEVCTRL_NIEN);

	assert(channel->sops->finish_dma);
	return channel->sops->finish_dma(channel);
}

/** SFF ATA channel ops. */
static ata_channel_ops_t ata_sff_ops = {
	.reset = ata_sff_reset,
	.status = ata_sff_status,
	.error = ata_sff_error,
	.selected = ata_sff_selected,
	.select = ata_sff_select,
	.command = ata_sff_command,
	.lba28_setup = ata_sff_lba28_setup,
	.lba48_setup = ata_sff_lba48_setup,
	.read_pio = ata_sff_read_pio,
	.write_pio = ata_sff_write_pio,
	.prepare_dma = ata_sff_prepare_dma,
	.start_dma = ata_sff_start_dma,
	.finish_dma = ata_sff_finish_dma,
};

/** Register a new SFF ATA channel.
 * @param parent	Parent in the device tree.
 * @param num		Channel number.
 * @param ops		Channel operations structure.
 * @param data		Implementation-specific data pointer.
 * @param dma		Whether the channel supports DMA.
 * @param max_dma_bpt	Maximum number of blocks per DMA transfer.
 * @param max_dma_addr	Maximum physical address for a DMA transfer.
 * @return		Pointer to channel structure if added, NULL if not. */
ata_channel_t *ata_sff_channel_add(device_t *parent, uint8_t num, ata_sff_channel_ops_t *ops,
                                   void *data, bool dma, size_t max_dma_bpt,
                                   phys_ptr_t max_dma_addr) {
	char name[DEVICE_NAME_MAX];
	sprintf(name, "ata%u", num);
	return ata_channel_add(parent, name, &ata_sff_ops, ops, data, 2, true, dma, max_dma_bpt, max_dma_addr);
}
MODULE_EXPORT(ata_sff_channel_add);
