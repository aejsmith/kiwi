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
 * @brief		Disk device manager internal definitions.
 */

#ifndef __DISK_PRIV_H
#define __DISK_PRIV_H

#include <drivers/disk.h>

#include <sync/mutex.h>

/** Partition information structure. */
typedef struct partition {
	list_t header;			/**< Link to partitions list for disk. */

	disk_device_t *parent;		/**< Disk device that partition is on. */
	device_t *device;		/**< Device node for the partition. */
	uint64_t offset;		/**< Starting block number. */
	uint64_t size;			/**< Size of partition in blocks. */
} partition_t;

/** Type of a partition scanner function.
 * @param device	Device to probe.
 * @return		Whether the disk matches this partition type. */
typedef bool (*disk_partition_probe_t)(disk_device_t *device);

extern bool disk_partition_probe_msdos(disk_device_t *device);

extern void disk_partition_probe(disk_device_t *device);
extern void disk_partition_add(disk_device_t *device, identifier_t id, uint64_t offset, uint64_t size);

extern int disk_device_read(disk_device_t *device, void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int disk_device_write(disk_device_t *device, const void *buf, size_t count, offset_t offset, size_t *bytesp);

#endif /* __DISK_PRIV_H */
