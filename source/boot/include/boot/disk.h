/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Bootloader disk functions.
 */

#ifndef __BOOT_DISK_H
#define __BOOT_DISK_H

#include <lib/list.h>

struct disk;
struct fs_mount;

/** Structure containing operations for a disk device. */
typedef struct disk_ops_t {
	/** Check if a partition is the boot partition.
	 * @param disk		Disk the partition is on.
	 * @param id		ID of partition.
	 * @param lba		Block that the partition starts at.
	 * @return		Whether partition is a boot partition. */
	bool (*is_boot_partition)(struct disk *disk, uint8_t id, uint64_t lba);

	/** Read blocks from the disk.
	 * @param disk		Disk being read from.
	 * @param buf		Buffer to read into.
	 * @param lba		Block number to start reading from.
	 * @param count		Number of blocks to read.
	 * @return		Whether reading succeeded. */
	bool (*read)(struct disk *disk, void *buf, uint64_t lba, size_t count);
} disk_ops_t;

/** Structure representing a disk device. */
typedef struct disk {
	list_t header;			/**< Link to device list. */

	char *name;			/**< Name of the device. */
	size_t block_size;		/**< Size of one block on the disk. */
	uint64_t blocks;		/**< Number of blocks on the disk. */
	disk_ops_t *ops;		/**< Pointer to operations structure. */
	struct fs_mount *fs;		/**< Filesystem that resides on the device. */
	union {
		struct {
			/** Implementation-specific data pointer. */
			void *data;

			/** Whether the disk is the boot disk. */
			bool boot;
		};
		struct {
			/** Parent of the partition. */
			struct disk *parent;

			/** Offset of the partition on the parent. */
			uint64_t offset;

			/** ID of the partition. */
			uint8_t id;
		};
	};
} disk_t;

extern disk_t *current_disk;

extern disk_t *disk_lookup(const char *str);
extern bool disk_read(disk_t *disk, void *buf, size_t count, offset_t offset);
extern void disk_partition_add(disk_t *parent, uint8_t id, uint64_t lba, uint64_t blocks);
extern void disk_add(const char *name, size_t block_size, uint64_t blocks, disk_ops_t *ops,
                     void *data, struct fs_mount *fs, bool boot);
extern disk_t *disk_parent(disk_t *disk);

extern void platform_disk_detect(void);
extern void disk_init(void);

#endif /* __BOOT_DISK_H */
