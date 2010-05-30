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
 * @brief		MSDOS partition table scanner.
 */

#ifndef __PARTITIONS_MSDOS_H
#define __PARTITIONS_MSDOS_H

#include <boot/fs.h>

/** MS-DOS partition table signature. */
#define MSDOS_SIGNATURE		0xAA55

/** MS-DOS partition description. */
typedef struct msdos_part {
	uint8_t  bootable;
	uint8_t  start_head;
	uint8_t  start_sector;
	uint8_t  start_cylinder;
	uint8_t  type;
	uint8_t  end_head;
	uint8_t  end_sector;
	uint8_t  end_cylinder;
	uint32_t start_lba;
	uint32_t num_sects;
} __packed msdos_part_t;

/** MS-DOS partition table. */
typedef struct msdos_mbr {
	uint8_t bootcode[446];
	msdos_part_t partitions[4];
	uint16_t signature;
} __packed msdos_mbr_t;

extern bool msdos_partition_probe(disk_t *disk);

#endif /* __PARTITIONS_MSDOS_H */
