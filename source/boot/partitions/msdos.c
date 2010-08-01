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

#include <boot/console.h>
#include <boot/memory.h>

#include <lib/utility.h>

#include "msdos.h"

/** Probe a disk for an MSDOS partition table.
 * @param disk		Device to scan.
 * @return		Whether an MSDOS partition table was found. */
bool msdos_partition_probe(disk_t *disk) {
	msdos_mbr_t *mbr = kmalloc(disk->block_size);
	msdos_part_t *part;
	size_t i;

	/* Read in the MBR, which is in the first block on the device. */
	if(!disk_read(disk, mbr, sizeof(msdos_mbr_t), 0) || mbr->signature != MSDOS_SIGNATURE) {
		kfree(mbr);
		return false;
	}

	/* Loop through all partitions in the table. */
	for(i = 0; i < ARRAYSZ(mbr->partitions); i++) {
		part = &mbr->partitions[i];
		if(part->type == 0 || (part->bootable != 0 && part->bootable != 0x80)) {
			continue;
		} else if(part->start_lba >= disk->blocks) {
			continue;
		} else if(part->start_lba + part->num_sects > disk->blocks) {
			continue;
		}

		dprintf("disk: found MSDOS partition %d on device %s\n", i, disk->name);
		dprintf(" type:      0x%x\n", part->type);
		dprintf(" start_lba: %u\n", part->start_lba);
		dprintf(" num_sects: %u\n", part->num_sects);

		disk_partition_add(disk, i, part->start_lba, part->num_sects);
	}

	kfree(mbr);
	return true;
}
