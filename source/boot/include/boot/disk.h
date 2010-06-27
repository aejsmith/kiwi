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
		};
	};
} disk_t;

extern disk_t *disk_lookup(const char *str);
extern bool disk_read(disk_t *disk, void *buf, uint64_t lba, size_t count);
extern void disk_partition_add(disk_t *parent, uint8_t id, uint64_t lba, uint64_t blocks);
extern void disk_add(char *name, size_t block_size, uint64_t blocks, disk_ops_t *ops,
                     void *data, bool boot);

extern void platform_disk_detect(void);
extern void disk_init(void);

#endif /* __BOOT_DISK_H */
