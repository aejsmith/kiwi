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
