/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Generic ATA device driver.
 *
 * Reference:
 * - PCI IDE Controller Specification
 *   http://suif.stanford.edu/~csapuntz/specs/pciide.ps
 * - AT Attachment with Packet Interface - 7: Volume 1
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 * - AT Attachment with Packet Interface - 7: Volume 2
 *   http://www.t13.org/Documents/UploadedDocuments/docs2007/
 */

#include <arch/io.h>

#include <drivers/pci.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <console.h>
#include <endian.h>
#include <kdbg.h>
#include <module.h>
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

/** Begin a block transfer.
 * @param device	Device to transfer on.
 * @param lba		Block number to start transfer at.
 * @param count		Number of blocks to transfer.
 * @return		Number of blocks that will be transferred. If 0 is
 *			returned, an error occurred. */
static size_t ata_device_transfer_begin(ata_device_t *device, uint64_t lba, size_t count) {
	ata_controller_t *controller = device->parent;

	/* The disk device layer should ensure that reads are within the size
	 * of the device. */
	assert((lba + count) <= device->blocks);

	if(lba < LBA28_MAX_BLOCK) {
		/* Check how many blocks we can transfer. */
		if((lba + count) > LBA28_MAX_BLOCK) {
			count = LBA28_MAX_BLOCK - lba;
		}
		if(count > UINT8_MAX) {
			count = UINT8_MAX;
		}

		/* Send a NULL to the feature register. */
		out8(controller->cmd_base + ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. */
		out8(controller->cmd_base + ATA_CMD_REG_COUNT, count);

		/* Specify the address of the block. */
		out8(controller->cmd_base + ATA_CMD_REG_LBA_LOW, lba & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Drive number and rest of address. */
		out8(controller->cmd_base + ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4) | ((lba >> 24) & 0xff));
		return count;
	} else if(lba < LBA48_MAX_BLOCK) {
		if(!(device->flags & ATA_DEVICE_LBA48)) {
			kprintf(LOG_WARN, "ata: attempted LBA48 read (%llu) on non-LBA48 device\n", lba);
			return 0;
		}

		/* Check how many blocks we can transfer. */
		if((lba + count) > LBA48_MAX_BLOCK) {
			count = LBA48_MAX_BLOCK - lba;
		}
		if(count > UINT16_MAX) {
			count = UINT16_MAX;
		}

		/* Send 2 NULLs to the feature register. */
		out8(controller->cmd_base + ATA_CMD_REG_FEAT, 0);
		out8(controller->cmd_base + ATA_CMD_REG_FEAT, 0);

		/* Write out the number of blocks to read. */
		out8(controller->cmd_base + ATA_CMD_REG_COUNT, (count >> 8) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_COUNT, count & 0xff);

		/* Specify the address of the block. */
		out8(controller->cmd_base + ATA_CMD_REG_LBA_LOW, (lba >> 24) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_LOW, lba & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_MID, (lba >> 32) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_MID, (lba >> 8) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_HIGH, (lba >> 40) & 0xff);
		out8(controller->cmd_base + ATA_CMD_REG_LBA_HIGH, (lba >> 16) & 0xff);

		/* Drive number. */
		out8(controller->cmd_base + ATA_CMD_REG_DEVICE, 0x40 | (device->num << 4));
		return count;
	} else {
		kprintf(LOG_WARN, "ata: attempted out of range transfer (%llu)\n", lba);
		return 0;
	}
}

/** Read from an ATA device.
 * @param _device	Device to read from.
 * @param buf		Buffer to read into.
 * @param lba		Block number to read from.
 * @param count		Number of blocks to read.
 * @return		Status code describing result of the operation. */
static status_t ata_device_read(disk_device_t *_device, void *buf, uint64_t lba, size_t count) {
	ata_device_t *device = _device->data;
	uint8_t cmd, error;
	size_t current;
	status_t ret;

	mutex_lock(&device->parent->lock);

	while(count) {
		/* Set up the address registers and select the device. */
		current = ata_device_transfer_begin(device, lba, count);
		if(!current) {
			mutex_unlock(&device->parent->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* For LBA48 transfers we must use READ SECTORS EXT. Do not
		 * need to check if LBA48 is supported because the previous
		 * function call picks up LBA48 addresses on non-LBA48
		 * devices. */
		cmd = (lba >= LBA28_MAX_BLOCK) ? ATA_CMD_READ_SECTORS_EXT : ATA_CMD_READ_SECTORS;

		/* Start the transfer and wait for it to complete. */
		ata_controller_command(device->parent, cmd);
		ret = ata_controller_wait(device->parent, ATA_STATUS_DRQ, 0, false, true, 10000000);
		if(ret != STATUS_SUCCESS) {
			error = ata_controller_error(device->parent);
			kprintf(LOG_WARN, "ata: read on device %" PRId32 ":%" PRIu8 " failed (status: %d, error: %" PRIu8 ")\n",
				device->parent->id, device->num, ret, error);
			mutex_unlock(&device->parent->lock);
			return ret;
		}

		/* Read the data. */
		ata_controller_pio_read(device->parent, buf, _device->block_size * current);
		buf += _device->block_size * current;
		count -= current;
		lba += current;
	}

	mutex_unlock(&device->parent->lock);
	return STATUS_SUCCESS;
}

/** Write to an ATA device.
 * @param _device	Device to write to.
 * @param buf		Buffer to write from.
 * @param lba		Block number to write to.
 * @param count		Number of blocks to write.
 * @return		Status code describing result of the operation. */
static status_t ata_device_write(disk_device_t *_device, const void *buf, uint64_t lba, size_t count) {
	ata_device_t *device = _device->data;
	uint8_t cmd, error;
	size_t current;
	status_t ret;

	mutex_lock(&device->parent->lock);

	while(count) {
		/* Set up the address registers and select the device. */
		current = ata_device_transfer_begin(device, lba, count);
		if(!current) {
			mutex_unlock(&device->parent->lock);
			return STATUS_DEVICE_ERROR;
		}

		/* For LBA48 transfers we must use WRITE SECTORS EXT. */
		cmd = (lba >= LBA28_MAX_BLOCK) ? ATA_CMD_WRITE_SECTORS_EXT : ATA_CMD_WRITE_SECTORS;

		/* Start the transfer and wait for it to complete. */
		ata_controller_command(device->parent, cmd);
		ret = ata_controller_wait(device->parent, ATA_STATUS_DRQ, 0, false, true, 10000000);
		if(ret != STATUS_SUCCESS) {
			error = ata_controller_error(device->parent);
			kprintf(LOG_WARN, "ata: write on device %" PRId32 ":%" PRIu8 " failed (status: %d, error: %" PRIu8 ")\n",
				device->parent->id, device->num, ret, error);
			mutex_unlock(&device->parent->lock);
			return ret;
		}

		/* Write the data. */
		ata_controller_pio_write(device->parent, buf, _device->block_size * current);
		buf += _device->block_size * current;
		count -= current;
		lba += current;
	}

	mutex_unlock(&device->parent->lock);
	return STATUS_SUCCESS;
}

/** Disk device operations structure. */
static disk_ops_t ata_device_ops = {
	.read = ata_device_read,
	.write = ata_device_write,
};

/** Detect a device on a controller.
 * @param controller	Controller to detect on.
 * @param num		Device number (0 or 1).
 * @return		Whether device was added. */
bool ata_device_detect(ata_controller_t *controller, uint8_t num) {
	char name[DEVICE_NAME_MAX];
	uint16_t *ident = NULL;
	ata_device_t *device;
	size_t blksize;
	status_t ret;

	mutex_lock(&controller->lock);

	/* Set the device. */
	ata_controller_select(controller, num);

	/* Send an IDENTIFY DEVICE command. */
	ata_controller_command(controller, ATA_CMD_IDENTIFY);
	if(ata_controller_wait(controller, ATA_STATUS_BSY | ATA_STATUS_DRQ, 0, true,
	                       true, 50000) != STATUS_SUCCESS) {
		goto fail;
	}

	/* Wait for data. */
	if(ata_controller_wait(controller, ATA_STATUS_DRQ, ATA_STATUS_BSY, false,
	                       true, 500000) != STATUS_SUCCESS) {
		goto fail;
	}

	/* Read in the identify data. */
	ident = kmalloc(512, MM_SLEEP);
	ata_controller_pio_read(controller, ident, 512);

	/* Check whether we can use the device. */
	if(le16_to_cpu(ident[0]) & (1<<15)) {
		kprintf(LOG_DEBUG, "ata: skipping non-ATA device %" PRId32 ":%" PRIu8 "\n", controller->id, num);
		goto fail;
	} else if(!(le16_to_cpu(ident[49]) & (1<<9))) {
		kprintf(LOG_DEBUG, "ata: skipping non-LBA device %" PRId32 ":%" PRIu8 "\n", controller->id, num);
		goto fail;
	}

	/* Allocate a device structure and fill it out. */
	device = kmalloc(sizeof(ata_device_t), MM_SLEEP);
	list_init(&device->header);
	device->num = num;
	device->parent = controller;
	device->flags = (le16_to_cpu(ident[83]) & (1<<10)) ? ATA_DEVICE_LBA48 : 0;
	device->blocks = le32_to_cpu(*(uint32_t *)(ident + 60));

	/* Get the block size - "Bit 12 of word 106 shall be set to 1 to
	 * indicate that the device has been formatted with a logical sector
	 * size larger than 256 words." */
	if(le16_to_cpu(ident[106]) & (1<<12)) {
		/* Words 117-118: Logical Sector Size. */
		blksize = le32_to_cpu(*(uint32_t *)(ident + 117)) * 2;
	} else {
		blksize = 512;
	}

	/* Copy information across. */
	ata_copy_string(device->model, (char *)(ident + 27), 40);
	ata_copy_string(device->serial, (char *)(ident + 10), 20);
	ata_copy_string(device->revision, (char *)(ident + 23), 8);

	kprintf(LOG_NORMAL, "ata: found device %" PRIu8 " on controller %" PRId32 ":\n", num, controller->id);
	kprintf(LOG_NORMAL, " model:     %s\n", device->model);
	kprintf(LOG_NORMAL, " serial:    %s\n", device->serial);
	kprintf(LOG_NORMAL, " revision:  %s\n", device->revision);
	kprintf(LOG_NORMAL, " flags:     %d\n", device->flags);
	kprintf(LOG_NORMAL, " blksize:   %u\n", blksize);
	kprintf(LOG_NORMAL, " blocks:    %u\n", device->blocks);
	kprintf(LOG_NORMAL, " size:      %llu\n", (uint64_t)device->blocks * blksize);

	mutex_unlock(&controller->lock);

	/* Register the device with the disk device manager. */
	sprintf(name, "%d", num);
	ret = disk_device_create(name, controller->device, &ata_device_ops, device,
	                         device->blocks, blksize, &device->device);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create ATA disk device %s (%d)", name, ret);
	}

	mutex_lock(&controller->lock);
	list_append(&controller->devices, &device->header);

	kfree(ident);
	mutex_unlock(&controller->lock);
	return true;
fail:
	if(ident) {
		kfree(ident);
	}
	mutex_unlock(&controller->lock);
	return false;
}
