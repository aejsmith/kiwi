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
	 * @return		Positive value on success, negative error code
	 *			on failure. */
	int (*request)(struct disk_device *device, int request, void *in, size_t insz, void **outp, size_t *outszp);

	/** Read a block from the device.
	 * @param device	Device to read from.
	 * @param buf		Buffer to read into.
	 * @param lba		Block number to read.
	 * @return		1 if read successfully, 0 if block doesn't
	 *			exist, negative error code on failure. */
	int (*block_read)(struct disk_device *device, void *buf, uint64_t lba);

	/** Write a block to the device.
	 * @param device	Device to write to.
	 * @param buf		Buffer to write from.
	 * @param lba		Block number to write.
	 * @return		1 if written successfully, 0 if block doesn't
	 *			exist, negative error code on failure. */
	int (*block_write)(struct disk_device *device, const void *buf, uint64_t lba);
} disk_ops_t;

/** Disk device information structure. */
typedef struct disk_device {
	mutex_t lock;			/**< Lock to protect structure. */
	identifier_t id;		/**< Device ID. */

	device_t *device;		/**< Device tree node. */
	device_t *alias;		/**< Alias if main device under different directory. */
	disk_ops_t *ops;		/**< Disk device operations structure. */
	void *data;			/**< Driver data structure. */
	size_t blksize;			/**< Block size of device. */

	list_t partitions;		/**< List of partitions on device. */
} disk_device_t;

extern int disk_device_create(const char *name, device_t *parent, disk_ops_t *ops,
                              void *data, size_t blksize, disk_device_t **devicep);
extern int disk_device_destroy(disk_device_t *device);

#endif /* __DRIVERS_DISK_H */
