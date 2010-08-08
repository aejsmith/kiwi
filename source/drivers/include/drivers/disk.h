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
 * @brief		Disk device manager.
 */

#ifndef __DRIVERS_DISK_H
#define __DRIVERS_DISK_H

#include <io/device.h>

struct disk_device;

/** Disk device operations structure. */
typedef struct disk_ops {
	/** Handler for device-specific requests.
	 * @note		This is called when a device request ID is
	 *			received that is greater than or equal to
	 *			DEVICE_CUSTOM_REQUEST_START.
	 * @param device	Device request is being made on.
	 * @param request	Request number.
	 * @param in		Input buffer.
	 * @param insz		Input buffer size.
	 * @param outp		Where to store pointer to output buffer.
	 * @param outszp	Where to store output buffer size.
	 * @return		Status code describing result of operation. */
	status_t (*request)(struct disk_device *device, int request, void *in, size_t insz,
	                    void **outp, size_t *outszp);

	/** Read blocks from the device.
	 * @param device	Device to read from.
	 * @param buf		Buffer to read into.
	 * @param lba		Block number to start from.
	 * @param count		Number of blocks to read.
	 * @return		Status code describing result of operation. */
	status_t (*read)(struct disk_device *device, void *buf, uint64_t lba, size_t count);

	/** Write a block to the device.
	 * @param device	Device to write to.
	 * @param buf		Buffer to write from.
	 * @param lba		Block number to start from.
	 * @param count		Number of blocks to read.
	 * @return		Status code describing result of operation. */
	status_t (*write)(struct disk_device *device, const void *buf, uint64_t lba, size_t count);
} disk_ops_t;

/** Disk device information structure. */
typedef struct disk_device {
	int id;				/**< Device ID. */
	disk_ops_t *ops;		/**< Disk device operations structure. */
	union {
		void *data;		/**< Implementation-specific data pointer. */
		uint64_t offset;	/**< Offset on the device (for partitions). */
	};
	uint64_t blocks;		/**< Number of blocks on the device. */
	size_t block_size;		/**< Block size of device. */
	device_t *device;		/**< Device tree node for the device. */
} disk_device_t;

extern status_t disk_device_create(const char *name, device_t *parent, disk_ops_t *ops,
                                   void *data, uint64_t blocks, size_t blksize,
                                   device_t **devicep);

#endif /* __DRIVERS_DISK_H */
