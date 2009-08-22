/* Kiwi device type/operation definitions
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
 * @brief		Device type/operation definitions.
 */

#ifndef __IO_DEVICE_TYPES_H
#define __IO_DEVICE_TYPES_H

#ifdef KERNEL
# include <types.h>
#else
# include <stdint.h>
#endif

/** Device type definitions. */
#define DEVICE_TYPE_BLOCK		1	/**< Block device. */
#define DEVICE_TYPE_INPUT		2	/**< Input device. */
#define DEVICE_TYPE_DISPLAY		3	/**< Display adapter. */
#define DEVICE_TYPE_NETWORK		4	/**< Network device. */

#if 0
# pragma mark Block device operations.
#endif

/** Structure containing arguments for a read/write block operation. */
typedef struct device_op_block_rw {
	uint64_t lba;				/**< Block number to read/write. */
	size_t count;				/**< Number of blocks to read/write. */
} device_op_block_rw_t;

/** Operations for block devices. */
#define DEVICE_OP_BLOCK_READ		1	/**< Read a block. */
#define DEVICE_OP_BLOCK_WRITE		2	/**< Write a block. */
#define DEVICE_OP_BLOCK_BLOCKSIZE	3	/**< Query the block size. */
#define DEVICE_OP_BLOCK_RESCAN		4	/**< Rescan the device for partitions. */

#endif /* __IO_DEVICE_TYPES_H */
