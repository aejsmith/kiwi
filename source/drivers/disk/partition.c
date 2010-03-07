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
 * @brief		Disk partition manager.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include "disk_priv.h"

/** Read from a partition device.
 * @param device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
static int partition_device_read(device_t *device, void *data, void *buf, size_t count,
                                 offset_t offset, size_t *bytesp) {
	partition_t *part = device->data;
	disk_device_t *disk = part->parent;

	if((uint64_t)(offset + count) >= (part->size * disk->blksize)) {
		*bytesp = 0;
		return 0;
	}

	return disk_device_read(part->parent, buf, count, offset + (part->offset * disk->blksize), bytesp);
}

/** Write to a device.
 * @param device	Device to write to.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
static int partition_device_write(device_t *device, void *data, const void *buf, size_t count,
                                  offset_t offset, size_t *bytesp) {
	partition_t *part = device->data;
	disk_device_t *disk = part->parent;

	if((uint64_t)(offset + count) >= (part->size * disk->blksize)) {
		*bytesp = 0;
		return 0;
	}

	return disk_device_write(part->parent, buf, count, offset + (part->offset * disk->blksize), bytesp);
}

/** Partition device operations. */
static device_ops_t partition_device_ops = {
	.read = partition_device_read,
	.write = partition_device_write,
};

/** Array of known partition types. */
static disk_partition_probe_t disk_partition_types[] = {
	disk_partition_probe_msdos,
};

/** Probe a disk for partitions.
 * @note		The partition list should be empty, and the device
 *			should not be locked.
 * @param device	Device to probe. */
void disk_partition_probe(disk_device_t *device) {
	size_t i;

	for(i = 0; i < ARRAYSZ(disk_partition_types); i++) {
		if(disk_partition_types[i](device)) {
			break;
		}
	}
}

/** Add a partition to a disk device.
 * @param device	Device to add to.
 * @param id		ID of the partition. Must be unique.
 * @param offset	Starting block number.
 * @param size		Size of partition in blocks. */
void disk_partition_add(disk_device_t *device, int id, uint64_t offset, uint64_t size) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "partition" } },
	};
	char name[DEVICE_NAME_MAX];
	partition_t *part;
	int ret;

	mutex_lock(&device->lock);

	part = kmalloc(sizeof(partition_t), MM_SLEEP);
	list_init(&part->header);
	part->parent = device;
	part->offset = offset;
	part->size = size;

	sprintf(name, "part%" PRId32, id);
	if((ret = device_create(name, device->device, &partition_device_ops, part,
	                        attrs, ARRAYSZ(attrs), &part->device)) != 0) {
		/* Should not fail - only failure is if name is non-unique,
		 * which it should be. */
		fatal("Could not create partition device (%d)", ret);
	}

	list_append(&device->partitions, &part->header);
	mutex_unlock(&device->lock);
}
