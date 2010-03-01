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
 * @brief		Disk device manager.
 */

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <errors.h>
#include <fatal.h>
#include <module.h>

#include "disk_priv.h"

/** Disk device directory. */
static device_t *disk_device_dir;

/** Next device ID. */
static atomic_t disk_next_id = 0;

/** Read from a disk device.
 * @param device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to read from.
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
static int disk_device__read(device_t *device, void *data, void *buf, size_t count,
                             offset_t offset, size_t *bytesp) {
	return disk_device_read(device->data, buf, count, offset, bytesp);
}

/** Write to a disk device.
 * @param device	Device to write to.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
static int disk_device__write(device_t *device, void *data, const void *buf, size_t count,
                              offset_t offset, size_t *bytesp) {
	return disk_device_write(device->data, buf, count, offset, bytesp);
}

/** Disk device operations structure. */
static device_ops_t disk_device_ops = {
	.read = disk_device__read,
	.write = disk_device__write,
	//.request = disk_device_request,
};

/** Read from a disk device.
 * @param device	Device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to read from.
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. */
int disk_device_read(disk_device_t *device, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	size_t total = 0, blksize = device->blksize;
	uint64_t start, end, i, size;
	void *block = NULL;
	int ret;

	if(!device->ops->block_read) {
		return -ERR_NOT_SUPPORTED;
	}

	/* Allocate a temporary buffer for partial transfers if required. */
	if(offset % blksize || count % blksize) {
		block = kmalloc(blksize, MM_SLEEP);
	}

	/* Now work out the start block and the end block. Subtract one from
	 * count to prevent end from going onto the next block when the offset
	 * plus the count is an exact multiple of the block size. */
	start = offset / blksize;
	end = (offset + (count - 1)) / blksize;

	/* If we're not starting on a block boundary, we need to do a partial
	 * transfer on the initial block to get up to a block boundary. 
	 * If the transfer only goes across one block, this will handle it. */
	if(offset % blksize) {
		/* Read the block into the temporary buffer. */
		if((ret = device->ops->block_read(device, block, start)) != 1) {
			goto out;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, block + (offset % blksize), size);
		total += size; buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	for(i = 0; i < size; i++, total += blksize, buf += blksize, count -= blksize, start++) {
		/* Read directly into the destination buffer. */
		if((ret = device->ops->block_read(device, buf, start)) != 1) {
			goto out;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = device->ops->block_read(device, block, start)) != 1) {
			goto out;
		}

		memcpy(buf, block, count);
		total += count;
	}

	ret = 0;
out:
	if(block) {
		kfree(block);
	}
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Write to a disk device.
 * @param device	Device to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
int disk_device_write(disk_device_t *device, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	size_t total = 0, blksize = device->blksize;
	uint64_t start, end, i, size;
	void *block = NULL;
	int ret;

	if(!device->ops->block_read || !device->ops->block_write) {
		return -ERR_NOT_SUPPORTED;
	}

	/* Allocate a temporary buffer for partial transfers if required. */
	if(offset % blksize || count % blksize) {
		block = kmalloc(blksize, MM_SLEEP);
	}

	/* Now work out the start page and the end block. Subtract one from
	 * count to prevent end from going onto the next block when the offset
	 * plus the count is an exact multiple of the block size. */
	start = offset / blksize;
	end = (offset + (count - 1)) / blksize;

	/* If we're not starting on a block boundary, we need to do a partial
	 * transfer on the initial block to get up to a block boundary. 
	 * If the transfer only goes across one block, this will handle it. */
	if(offset % blksize) {
		/* Slightly more difficult than the read case, we must read
		 * the block in, partially overwrite it and write back. */
		if((ret = device->ops->block_read(device, block, start)) != 1) {
			goto out;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(block + (offset % blksize), buf, size);

		if((ret = device->ops->block_write(device, block, start)) != 1) {
			goto out;
		}

		total += size; buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	for(i = 0; i < size; i++, total += blksize, buf += blksize, count -= blksize, start++) {
		if((ret = device->ops->block_write(device, buf, start)) != 1) {
			goto out;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = device->ops->block_read(device, block, start)) != 1) {
			goto out;
		}

		memcpy(block, buf, count);

		if((ret = device->ops->block_write(device, block, start)) != 1) {
			goto out;
		}

		total += count;
	}

	ret = 0;
out:
	if(block) {
		kfree(block);
	}
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Create a new disk device.
 *
 * Registers a new disk device with the disk device manager.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the disk device container.
 * @param ops		Disk device operations structure.
 * @param data		Data used by driver.
 * @param blksize	Block size of device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		0 on success, negative error code on failure. Only
 *			possible failure is if name already exists in parent.
 */
int disk_device_create(const char *name, device_t *parent, disk_ops_t *ops,
                       void *data, size_t blksize, disk_device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "disk" } },
		{ "disk.block-size", DEVICE_ATTR_UINT32, { .uint32 = blksize } },
	};
	char dname[DEVICE_NAME_MAX];
	disk_device_t *device;
	int ret;

	if((parent && !name) || (name && !parent) || !ops || !blksize || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	device = kmalloc(sizeof(disk_device_t), MM_SLEEP);
	mutex_init(&device->lock, "disk_device_lock", 0);
	list_init(&device->partitions);
	device->id = atomic_inc(&disk_next_id);
	device->ops = ops;
	device->data = data;
	device->blksize = blksize;

	/* Create the device tree node. */
	sprintf(dname, "%" PRId32, device->id);
	if(parent) {
		if((ret = device_create(name, parent, &disk_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		} else if((ret = device_alias(dname, disk_device_dir, device->device, &device->alias)) != 0) {
			/* Should not fail - only possible failure is if name
			 * already exists, and ID should be unique. Note that
			 * with current ID allocation implementation this can
			 * happen - FIXME. */
			fatal("Could not create device alias (%d)", ret);
		}
	} else {
		if((ret = device_create(dname, disk_device_dir, &disk_device_ops, device, attrs,
	                                ARRAYSZ(attrs), &device->device)) != 0) {
			kfree(device);
			return ret;
		}
		device->alias = NULL;
	}

	/* Probe for partitions on the device. */
	disk_partition_probe(device);

	*devicep = device;
	return 0;
}
MODULE_EXPORT(disk_device_create);

/** Destroy a disk device.
 *
 * Removes a disk device and all partitions under it from the device tree.
 *
 * @param device	Device to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int disk_device_destroy(disk_device_t *device) {
	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(disk_device_destroy);

/** Initialisation function for the disk module.
 * @return		0 on success, negative error code on failure. */
static int disk_init(void) {
	/* Create the disk device directory. */
	return device_create("disk", device_tree_root, NULL, NULL, NULL, 0, &disk_device_dir);
}

/** Unloading function for the disk module.
 * @return		0 on success, negative error code on failure. */
static int disk_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("disk");
MODULE_DESC("Disk device class manager");
MODULE_FUNCS(disk_init, disk_unload);
