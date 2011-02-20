/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		MSDOS partition table scanner.
 */

#ifndef __PARTITIONS_MSDOS_H
#define __PARTITIONS_MSDOS_H

#include <disk.h>

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
