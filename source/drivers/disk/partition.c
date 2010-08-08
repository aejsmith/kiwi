/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Disk partition manager.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <console.h>
#include <status.h>

#include "disk_priv.h"

/** Array of known partition types. */
static partition_probe_t partition_types[] = {
	partition_probe_msdos,
};

/** Read from a partition device.
 * @param _device	Device to read from.
 * @param buf		Buffer to read into.
 * @param lba		Block number to start from.
 * @param count		Number of blocks to read.
 * @return		Status code describing result of the operation. */
static status_t partition_disk_read(disk_device_t *device, void *buf, uint64_t lba, size_t count) {
	disk_device_t *parent = device->device->parent->data;
	if(parent->ops->read) {
		return parent->ops->read(parent, buf, lba + device->offset, count);
	} else {
		return STATUS_NOT_SUPPORTED;
	}
}

/** Write to a partition device.
 * @param _device	Device to write to.
 * @param buf		Buffer to write from.
 * @param lba		Block number to start from.
 * @param count		Number of blocks to write.
 * @return		Status code describing result of the operation. */
static status_t partition_disk_write(disk_device_t *device, const void *buf, uint64_t lba, size_t count) {
	disk_device_t *parent = device->device->parent->data;
	if(parent->ops->write) {
		return parent->ops->write(parent, buf, lba + device->offset, count);
	} else {
		return STATUS_NOT_SUPPORTED;
	}
}

/** Partition device operations. */
static disk_ops_t partition_disk_ops = {
	.read = partition_disk_read,
	.write = partition_disk_write,
};

/** Probe a disk for partitions.
 * @param device	Device to probe. */
void partition_probe(disk_device_t *device) {
	size_t i;

	for(i = 0; i < ARRAYSZ(partition_types); i++) {
		if(partition_types[i](device)) {
			break;
		}
	}
}

/** Add a partition to a disk device.
 * @param parent	Device to add to.
 * @param id		ID of the partition. Must be unique.
 * @param offset	Starting block number.
 * @param size		Size of partition in blocks. */
void partition_add(disk_device_t *parent, int id, uint64_t offset, uint64_t size) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "disk" } },
		{ "disk.blocks", DEVICE_ATTR_UINT64, { .uint64 = size } },
		{ "disk.block-size", DEVICE_ATTR_UINT32, { .uint32 = parent->block_size } },
	};
	char name[DEVICE_NAME_MAX];
	disk_device_t *device;
	status_t ret;

	device = kmalloc(sizeof(disk_device_t), MM_SLEEP);
	device->id = id;
	device->ops = &partition_disk_ops;
	device->offset = offset;
	device->blocks = size;
	device->block_size = parent->block_size;

	/* Create the device tree node. */
	sprintf(name, "%d", device->id);
	ret = device_create(name, parent->device, &disk_device_ops, device, attrs,
	                    ARRAYSZ(attrs), &device->device);
	if(ret != STATUS_SUCCESS) {
		kfree(device);
	}
}
