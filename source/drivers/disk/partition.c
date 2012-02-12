/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Disk partition manager.
 */

#include <io/fs.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <kernel.h>
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
 * @param device	Device to probe.
 * @return		Whether any partitions were found. */
bool partition_probe(disk_device_t *device) {
	size_t i;

	for(i = 0; i < ARRAY_SIZE(partition_types); i++) {
		if(partition_types[i](device)) {
			return true;
		}
	}

	return false;
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

	device = kmalloc(sizeof(disk_device_t), MM_WAIT);
	device->id = id;
	device->ops = &partition_disk_ops;
	device->offset = offset;
	device->blocks = size;
	device->block_size = parent->block_size;

	/* Create the device tree node. */
	sprintf(name, "%d", device->id);
	ret = device_create(name, parent->device, &disk_device_ops, device, attrs,
	                    ARRAY_SIZE(attrs), &device->device);
	if(ret != STATUS_SUCCESS) {
		kfree(device);
	}

	/* Probe the partition for filesystems. */
	fs_probe(device->device);
}
