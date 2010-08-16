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
 * @brief		ATA device functions.
 *
 * Reference:
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <endian.h>
#include <status.h>

#include "ata_priv.h"

/** Highest block number for LBA-28 transfers. */
#define LBA28_MAX_BLOCK		((uint64_t)1<<28)

/** Highest block number for LBA-28 transfers. */
#define LBA48_MAX_BLOCK		((uint64_t)1<<48)

/** Copy an ATA identification string.
 * @note		Modifies the source string.
 * @param dest		Destination string.
 * @param src		Source string.
 * @param size		Size of the string. */
static void ata_copy_string(char *dest, char *src, size_t size) {
	char *ptr, tmp;
	size_t i;

	ptr = src;
	for(i = 0; i < size; i += 2) {
		tmp = *ptr;
		*ptr = *(ptr + 1);
		ptr++;
		*ptr = tmp;
		ptr++;
	}

	/* Get rid of the trailing spaces. */
	for(i = size - 1; i > 0; i--) {
		if(src[i] != ' ') {
			break;
		}
	}

	memcpy(dest, src, i + 1);
	dest[i + 1] = 0;
}

/** Begin an I/O operation.
 * @param device	Device to transfer on.
 * @param lba		Block number to start transfer at.
 * @param count		Number of blocks to transfer.
 * @return		Number of blocks that will be transferred. If 0 is
 *			returned, an error occurred. */
static size_t ata_device_begin_io(ata_device_t *device, uint64_t lba, size_t count) {
	ata_channel_t *channel = device->parent;

	if(lba < LBA28_MAX_BLOCK) {
		/* Check how many blocks we can transfer. */
		if((lba + count) > LBA28_MAX_BLOCK) {
			count = LBA28_MAX_BLOCK - lba;
		}
		if(count > 256) {
			count = 256;
		}

		/* Send a NULL to the feature register. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. 0 means 256. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, (count == 256) ? 0 : count);

		/* Specify the address of the block. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Device number with LBA bit set, and last 4 bits of address. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4) | ((lba >> 24) & 0xf));
		return count;
	} else if(lba < LBA48_MAX_BLOCK) {
		if(!device->lba48) {
			kprintf(LOG_WARN, "ata: attempted LBA48 read (%" PRIu64 ") on non-LBA48 device\n", lba);
			return 0;
		}

		/* Check how many blocks we can transfer. */
		if((lba + count) > LBA48_MAX_BLOCK) {
			count = LBA48_MAX_BLOCK - lba;
		}
		if(count > 65536) {
			count = 65536;
		}

		/* Send 2 NULLs to the feature register. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);
		ata_channel_write_cmd(channel, ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. */
		if(count == 65536) {
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, 0);
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, 0);
		} else {
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, (count >> 8) & 0xff);
			ata_channel_write_cmd(channel, ATA_CMD_REG_COUNT, count & 0xff);
		}

		/* Specify the address of the block. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, (lba >> 24) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_LOW, lba & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 32) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 40) & 0xff);
		ata_channel_write_cmd(channel, ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Device number with LBA bit set. */
		ata_channel_write_cmd(channel, ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4));
		return count;
	} else {
		kprintf(LOG_WARN, "ata: attempted out of range transfer (%" PRIu64 ")\n", lba);
		return 0;
	}
}

/** Read from an ATA disk.
 * @param _device	Device to read from.
 * @param buf		Buffer to read into.
 * @param lba		Block number to read from.
 * @param count		Number of blocks to read.
 * @return		Status code describing result of the operation. */
static status_t ata_disk_read(disk_device_t *_device, void *buf, uint64_t lba, size_t count) {
	ata_device_t *device = _device->data;
	uint8_t cmd, error, status;
	size_t current, i;
	status_t ret;

	ata_channel_begin_command(device->parent, device->num);

	while(count) {
		/* Set up the address registers and select the device. */
		current = ata_device_begin_io(device, lba, count);
		if(!current) {
			ata_channel_finish_command(device->parent);
			return STATUS_DEVICE_ERROR;
		}

		/* For LBA48 transfers we must use READ SECTORS EXT. Do not
		 * need to check if LBA48 is supported because the previous
		 * function call picks up LBA48 addresses on non-LBA48
		 * devices. */
		cmd = (lba >= LBA28_MAX_BLOCK) ? ATA_CMD_READ_SECTORS_EXT : ATA_CMD_READ_SECTORS;

		/* Start the transfer. */
		ata_channel_command(device->parent, cmd);

		/* Transfer each sector. */
		for(i = 0; i < current; i++) {
			ret = ata_channel_read_pio(device->parent, buf, _device->block_size);
			if(ret != STATUS_SUCCESS) {
				status = ata_channel_status(device->parent);
				error = ata_channel_error(device->parent);
				kprintf(LOG_WARN, "ata: read of %zu block(s) from %" PRIu64 " "
				        "on %d:%u failed on block %zu (ret: %d, status: %u, "
				        "error: %u)\n", current, lba, device->parent->id,
				        device->num, i, ret, status, error);
				ata_channel_finish_command(device->parent);
				return STATUS_DEVICE_ERROR;
			}
			buf += _device->block_size;
		}

		count -= current;
		lba += current;
	}

	ata_channel_finish_command(device->parent);
	return STATUS_SUCCESS;
}

/** Write to an ATA device.
 * @param _device	Device to write to.
 * @param buf		Buffer to write from.
 * @param lba		Block number to write to.
 * @param count		Number of blocks to write.
 * @return		Status code describing result of the operation. */
static status_t ata_disk_write(disk_device_t *_device, const void *buf, uint64_t lba, size_t count) {
#if 0
	ata_device_t *device = _device->data;
	uint8_t cmd, error, status;
	size_t current, i;
	status_t ret;

	mutex_lock(&device->parent->lock);

	while(count) {
		/* Set up the address registers and select the device. */
		current = ata_device_begin_transfer(device, lba, count);
		if(!current) {
			mutex_unlock(&device->parent->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* For LBA48 transfers we must use WRITE SECTORS EXT. */
		cmd = (lba >= LBA28_MAX_BLOCK) ? ATA_CMD_WRITE_SECTORS_EXT : ATA_CMD_WRITE_SECTORS;

		/* Start the transfer. */
		ata_controller_command(device->parent, cmd);

		/* Transfer each sector. */
		for(i = 0; i < current; i++) {
			ret = ata_controller_wait(device->parent, ATA_STATUS_DRQ, 0, false, true, 10000000);
			if(ret != STATUS_SUCCESS) {
				status = ata_controller_status(device->parent);
				error = ata_controller_error(device->parent);
				kprintf(LOG_WARN, "ata: write of %zu block(s) on %" PRId32 ":%" PRIu8 " failed "
				        "on block %zu (ret: %d, status: %" PRIu8 ", error: %" PRIu8 ")\n",
				        current, device->parent->id, device->num, i, ret, status, error);
				mutex_unlock(&device->parent->lock);
				return ret;
			}

			/* Write the sector. */
			ata_controller_pio_write(device->parent, buf, _device->block_size);
			buf += _device->block_size;
		}

		count -= current;
		lba += current;
	}

	mutex_unlock(&device->parent->lock);
	return STATUS_SUCCESS;
#endif
	return STATUS_NOT_IMPLEMENTED;
}

/** ATA disk device operations structure. */
static disk_ops_t ata_disk_ops = {
	.read = ata_disk_read,
	.write = ata_disk_write,
};

/** Detect a device on a channel.
 * @param channel	Channel to detect on.
 * @param num		Device number (0 or 1). */
void ata_device_detect(ata_channel_t *channel, uint8_t num) {
	uint16_t *ident = NULL, word;
	char name[DEVICE_NAME_MAX];
	size_t block_size, blocks;
	ata_device_t *device;
	status_t ret;

	if(ata_channel_begin_command(channel, num) != STATUS_SUCCESS) {
		return;
	}

	/* Send an IDENTIFY DEVICE command. Perform a manual wait as we don't
	 * want to wait too long if the device doesn't exist. */
	ident = kmalloc(512, MM_SLEEP);
	ata_channel_command(channel, ATA_CMD_IDENTIFY);
	if(ata_channel_wait(channel, ATA_STATUS_BSY | ATA_STATUS_DRQ, 0, true,
	                    true, 50000) != STATUS_SUCCESS) {
		goto fail;
	} else if(ata_channel_read_pio(channel, ident, 512) != STATUS_SUCCESS) {
		goto fail;
	}

	/* Check whether we can use the device. */
	if(le16_to_cpu(ident[0]) & (1<<15)) {
		kprintf(LOG_DEBUG, "ata: skipping non-ATA device %d:%u\n", channel->id, num);
		goto fail;
	} else if(!(le16_to_cpu(ident[49]) & (1<<9))) {
		kprintf(LOG_DEBUG, "ata: skipping non-LBA device %d:%u\n", channel->id, num);
		goto fail;
	}

	/* Allocate a device structure and fill it out. */
	device = kmalloc(sizeof(*device), MM_SLEEP);
	device->num = num;
	device->parent = channel;
	if(le16_to_cpu(ident[83]) & (1<<10)) {
		device->lba48 = true;
	}
	// FIXME: DMA
	device->dma = false;

	/* Get the block count. */
	blocks = le32_to_cpu(*(uint32_t *)(ident + 60));

	/* Get the block size - "Bit 12 of word 106 shall be set to 1 to
	 * indicate that the device has been formatted with a logical sector
	 * size larger than 256 words." */
	word = le16_to_cpu(ident[106]);
	if(word & (1<<14) && !(word & (1<<15)) && word & (1<<12)) {
		/* Words 117-118: Logical Sector Size. */
		block_size = le32_to_cpu(*(uint32_t *)(ident + 117)) * 2;
	} else {
		block_size = 512;
	}

	/* Copy information across. */
	ata_copy_string(device->model, (char *)(ident + 27), 40);
	ata_copy_string(device->serial, (char *)(ident + 10), 20);
	ata_copy_string(device->revision, (char *)(ident + 23), 8);

	kprintf(LOG_NORMAL, "ata: found device %u on channel %d:\n", num, channel->id);
	kprintf(LOG_NORMAL, " model:      %s\n", device->model);
	kprintf(LOG_NORMAL, " serial:     %s\n", device->serial);
	kprintf(LOG_NORMAL, " revision:   %s\n", device->revision);
	kprintf(LOG_NORMAL, " lba48:      %d\n", device->lba48);
	//FIXME more info kprintf(LOG_NORMAL, " dma:      %d\n", device->dma);
	kprintf(LOG_NORMAL, " block_size: %u\n", block_size);
	kprintf(LOG_NORMAL, " blocks:     %u\n", blocks);
	kprintf(LOG_NORMAL, " size:       %llu\n", (uint64_t)blocks * block_size);

	kfree(ident);
	ata_channel_finish_command(channel);

	/* Register the device with the disk device manager. */
	sprintf(name, "%d", num);
	ret = disk_device_create(name, channel->node, &ata_disk_ops, device, blocks,
	                         block_size, &device->node);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create ATA disk device %u (%d)", num, ret);
	}
	return;
fail:
	kfree(ident);
	ata_channel_finish_command(channel);
}
