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
 * @brief		Disk device manager.
 */

#include <io/fs.h>

#include <lib/atomic.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <kernel.h>
#include <module.h>
#include <status.h>

#include "disk_priv.h"

/** Disk device directory. */
static device_t *disk_device_dir;

/** Next device ID. */
static atomic_t next_disk_id = 0;

/** Destroy a disk device.
 * @param _device	Device to destroy. */
static void disk_device_destroy(device_t *_device) {
	/* TODO: I'm lazy. */
	kprintf(LOG_WARN, "disk: destroy is not implemented, happily leaking a bunch of memory!\n");
}

/** Read from a disk device.
 * @param _device	Device to read from.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to read from.
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t disk_device_read_(device_t *_device, void *data, void *buf, size_t count,
                                  offset_t offset, size_t *bytesp) {
	disk_device_t *device = _device->data;
	return disk_device_read(device, buf, count, offset, bytesp);
}

/** Write to a disk device.
 * @param _device	Device to write to.
 * @param data		Handle-specific data pointer (unused).
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t disk_device_write_(device_t *_device, void *data, const void *buf, size_t count,
                                   offset_t offset, size_t *bytesp) {
	disk_device_t *device = _device->data;
	return disk_device_write(device, buf, count, offset, bytesp);
}

/** Disk device operations structure. */
device_ops_t disk_device_ops = {
	.destroy = disk_device_destroy,
	.read = disk_device_read_,
	.write = disk_device_write_,
};

/** Read from a disk device.
 * @param device	Device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset to read from.
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
status_t disk_device_read(disk_device_t *device, void *buf, size_t count, offset_t offset,
                          size_t *bytesp) {
	size_t total = 0, blksize = device->block_size;
	status_t ret = STATUS_SUCCESS;
	uint64_t start, end, size;
	void *block = NULL;

	if(!device->ops || !device->ops->read) {
		return STATUS_NOT_SUPPORTED;
	} else if(!count || offset >= (device->blocks * device->block_size)) {
		goto out;
	} else if((offset + count) > (device->blocks * device->block_size)) {
		count = (device->blocks * device->block_size) - offset;
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
		ret = device->ops->read(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, block + (offset % blksize), size);
		total += size; buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	if(size) {
		ret = device->ops->read(device, buf, start, size);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		total += (size * blksize);
		buf += (size * blksize);
		count -= (size * blksize);
		start += size;
	}

	/* Handle anything that's left. */
	if(count > 0) {
		ret = device->ops->read(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		memcpy(buf, block, count);
		total += count;
	}

	ret = STATUS_SUCCESS;
out:
	if(block) {
		kfree(block);
	}
	*bytesp = total;
	return ret;
}

/** Write to a disk device.
 * @param device	Device to write to.
 * @param buf		Buffer to write from.
 * @param count		Number of bytes to write.
 * @param offset	Offset to write to.
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
status_t disk_device_write(disk_device_t *device, const void *buf, size_t count,
                           offset_t offset, size_t *bytesp) {
	size_t total = 0, blksize = device->block_size;
	status_t ret = STATUS_SUCCESS;
	uint64_t start, end, size;
	void *block = NULL;

	if(!device->ops || !device->ops->read || !device->ops->write) {
		return STATUS_NOT_SUPPORTED;
	} else if(!count || offset >= (device->blocks * device->block_size)) {
		goto out;
	} else if((offset + count) > (device->blocks * device->block_size)) {
		count = (device->blocks * device->block_size) - offset;
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
		/* Slightly more difficult than the read case, we must read
		 * the block in, partially overwrite it and write back. */
		ret = device->ops->read(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(block + (offset % blksize), buf, size);

		ret = device->ops->write(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		total += size; buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	if(size) {
		ret = device->ops->write(device, buf, start, size);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		total += (size * blksize);
		buf += (size * blksize);
		count -= (size * blksize);
		start += size;
	}

	/* Handle anything that's left. */
	if(count > 0) {
		ret = device->ops->read(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		memcpy(block, buf, count);

		ret = device->ops->write(device, block, start, 1);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		total += count;
	}

	ret = STATUS_SUCCESS;
out:
	if(block) {
		kfree(block);
	}
	*bytesp = total;
	return ret;
}

/**
 * Create a new disk device.
 *
 * Registers a new disk device with the disk device manager and scans it for
 * partitions.
 *
 * @param name		Name to give device. Only used if parent is specified.
 * @param parent	Optional parent node. If not provided, then the main
 *			device will be created under the disk device container.
 * @param ops		Disk device operations structure.
 * @param data		Data used by driver.
 * @param blksize	Block size of device.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t disk_device_create(const char *name, device_t *parent, disk_ops_t *ops,
                            void *data, uint64_t blocks, size_t blksize,
                            device_t **devicep) {
	device_attr_t attrs[] = {
		{ "type", DEVICE_ATTR_STRING, { .string = "disk" } },
		{ "disk.blocks", DEVICE_ATTR_UINT64, { .uint64 = blocks } },
		{ "disk.block-size", DEVICE_ATTR_UINT32, { .uint32 = blksize } },
	};
	char dname[DEVICE_NAME_MAX];
	disk_device_t *device;
	status_t ret;

	if((parent && !name) || (name && !parent) || !ops || !blocks || !blksize || !devicep) {
		return STATUS_INVALID_ARG;
	}

	device = kmalloc(sizeof(disk_device_t), MM_SLEEP);
	device->id = atomic_inc(&next_disk_id);
	device->ops = ops;
	device->data = data;
	device->blocks = blocks;
	device->block_size = blksize;

	/* Create the device tree node. */
	sprintf(dname, "%d", device->id);
	if(parent) {
		ret = device_create(name, parent, &disk_device_ops, device, attrs,
		                    ARRAY_SIZE(attrs), &device->device);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}

		/* Should not fail - only possible failure is if name already
		 * exists, and ID should be unique. */
		device_alias(dname, disk_device_dir, device->device, NULL);
	} else {
		ret = device_create(dname, disk_device_dir, &disk_device_ops, device, attrs,
		                    ARRAY_SIZE(attrs), &device->device);
		if(ret != STATUS_SUCCESS) {
			kfree(device);
			return ret;
		}
	}

	/* Probe for partitions/filesystems on the device. */
	if(!partition_probe(device)) {
		fs_probe(device->device);
	}
	*devicep = device->device;
	return STATUS_SUCCESS;
}
MODULE_EXPORT(disk_device_create);

/** Initialisation function for the disk module.
 * @return		Status code describing result of the operation. */
static status_t disk_init(void) {
	/* Create the disk device directory. */
	return device_create("disk", device_tree_root, NULL, NULL, NULL, 0, &disk_device_dir);
}

/** Unloading function for the disk module.
 * @return		Status code describing result of the operation. */
static status_t disk_unload(void) {
	return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME("disk");
MODULE_DESC("Disk device class manager");
MODULE_FUNCS(disk_init, disk_unload);
