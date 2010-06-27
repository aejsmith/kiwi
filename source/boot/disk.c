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
 * @brief		Bootloader disk functions.
 */

#include <boot/disk.h>
#include <boot/fs.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <fatal.h>

#include "partitions/msdos.h"

/** List of all disk devices. */
static LIST_DECLARE(disk_list);

/** Array of partition probe functions. */
static bool (*partition_probe_funcs[])(disk_t *) = {
	msdos_partition_probe,
};

/** Look up a disk according to a string.
 *
 * Looks up a disk according to the given string. If the string is in the form
 * "(<name>)", then the disk will be looked up by its name. Otherwise, the
 * string will be taken as a UUID, and the disk containing a filesystem with
 * that UUID will be returned.
 *
 * @param str		String for lookup.
 *
 * @return		Pointer to disk structure if found, NULL if not.
 */
disk_t *disk_lookup(const char *str) {
	disk_t *disk;

	LIST_FOREACH(&disk_list, iter) {
		disk = list_entry(iter, disk_t, header);

		if(str[0] == '(') {
			if(strncmp(disk->name, str + 1, strlen(str) - 2) == 0) {
				return disk;
			}
		} else if(disk->fs && disk->fs->uuid) {
			if(strcmp(disk->fs->uuid, str) == 0) {
				return disk;
			}
		}
	}

	return NULL;
}

/** Read from a disk.
 * @param disk		Disk to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the disk to read from.
 * @return		Whether the read was successful. */
bool disk_read(disk_t *disk, void *buf, size_t count, offset_t offset) {
	size_t blksize = disk->block_size;
	uint64_t start, end, size;
	void *block = NULL;

	if(!count) {
		return true;
	}

	/* Allocate a temporary buffer for partial transfers if required. */
	if(offset % blksize || count % blksize) {
		block = kmalloc(blksize);
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
		if(!disk->ops->read(disk, block, start, 1)) {
			kfree(block);
			return false;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, block + (offset % blksize), size);
		buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	if(size) {
		if(!disk->ops->read(disk, buf, start, size)) {
			if(block) {
				kfree(block);
			}
			return false;
		}
		buf += (size * blksize);
		count -= (size * blksize);
		start += size;
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!disk->ops->read(disk, block, start, 1)) {
			kfree(block);
			return false;
		}

		memcpy(buf, block, count);
	}

	if(block) {
		kfree(block);
	}
	return true;
}

/** Probe a disk for filesystems/partitions.
 * @param disk		Disk to probe. */
static void disk_probe(disk_t *disk) {
	size_t i;

	if(!(disk->fs = fs_probe(disk))) {
		for(i = 0; i < ARRAYSZ(partition_probe_funcs); i++) {
			if(partition_probe_funcs[i](disk)) {
				return;
			}
		}
	}
}

/** Read blocks from a partition.
 * @param disk		Disk being read from.
 * @param buf		Buffer to read into.
 * @param lba		Starting block number.
 * @param count		Number of blocks to read.
 * @return		Whether reading succeeded. */
static bool partition_disk_read(disk_t *disk, void *buf, uint64_t lba, size_t count) {
	return disk->parent->ops->read(disk->parent, buf, lba + disk->offset, count);
}

/** Operations for a partition disk. */
static disk_ops_t partition_disk_ops = {
	.read = partition_disk_read,
};

/** Add a partition to a disk device.
 * @param parent	Parent of the partition.
 * @param id		Partition number.
 * @param lba		Offset into parent device of the partition.
 * @param blocks	Size of the partition in blocks. */
void disk_partition_add(disk_t *parent, uint8_t id, uint64_t lba, uint64_t blocks) {
	disk_t *disk = kmalloc(sizeof(disk_t));
	char name[32];

	sprintf(name, "%s,%u", parent->name, id);
	list_init(&disk->header);
	disk->name = kstrdup(name);
	disk->block_size = parent->block_size;
	disk->blocks = blocks;
	disk->ops = &partition_disk_ops;
	disk->fs = NULL;
	disk->parent = parent;
	disk->offset = lba;

	/* Probe for filesystems/partitions. */
	disk_probe(disk);
	if(disk->fs && parent->boot && parent->ops->is_boot_partition) {
		if(parent->ops->is_boot_partition(parent, id, lba)) {
			boot_filesystem = disk->fs;
		}
	}

	list_append(&disk_list, &disk->header);
}

/** Register a disk device.
 * @param name		Name of the disk (should be a kmalloc()'d string).
 * @param block_size	Size of 1 block on the device.
 * @param blocks	Number of blocks on the device.
 * @param ops		Operations structure.
 * @param data		Implementation-specific data pointer.
 * @param boot		Whether the disk is the boot disk. */
void disk_add(char *name, size_t block_size, uint64_t blocks, disk_ops_t *ops, void *data, bool boot) {
	disk_t *disk = kmalloc(sizeof(disk_t));

	list_init(&disk->header);
	disk->name = name;
	disk->block_size = block_size;
	disk->blocks = blocks;
	disk->ops = ops;
	disk->fs = NULL;
	disk->data = data;
	disk->boot = boot;

	/* Probe for filesystems/partitions. */
	disk_probe(disk);
	if(disk->fs && boot) {
		boot_filesystem = disk->fs;
	}

	list_append(&disk_list, &disk->header);
}

/** Detect all disk devices. */
void disk_init(void) {
	platform_disk_detect();
	if(!boot_filesystem) {
		fatal("Could not find boot filesystem");
	}
}
