/* Kiwi MSDOS partition table scanner
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
 * @brief		MSDOS partition table scanner.
 */

#include <console/kprintf.h>

#include <lib/utility.h>

#include <mm/malloc.h>

#include "disk_priv.h"

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
} __attribute__((packed)) msdos_part_t;

/** MS-DOS partition table. */
typedef struct msdos_mbr {
	uint8_t bootcode[446];
	msdos_part_t partitions[4];
	uint16_t signature;
} __attribute__((packed)) msdos_mbr_t;

/** Probe a disk for an MSDOS partition table.
 * @param device	Device to scan.
 * @return		Whether an MSDOS partition table was found. */
bool disk_partition_probe_msdos(disk_device_t *device) {
	msdos_mbr_t *mbr = kmalloc(sizeof(msdos_mbr_t), MM_SLEEP);
	size_t bytes, i;
	int ret;

	if((ret = disk_device_read(device, mbr, sizeof(msdos_mbr_t), 0, &bytes)) != 0 || bytes != sizeof(msdos_mbr_t)) {
		kprintf(LOG_DEBUG, "disk: could not read MSDOS MBR from %p (%d)\n",
			device, ret);
		kfree(mbr);
		return false;
	} else if(mbr->signature != MSDOS_SIGNATURE) {
		kfree(mbr);
		return false;
	}

	/* Loop through all partitions in the table. */
	for(i = 0; i < ARRAYSZ(mbr->partitions); i++) {
		if(mbr->partitions[i].type == 0) {
			continue;
		}

		kprintf(LOG_DEBUG, "disk: found MSDOS partition %d on device %p\n", i, device);
		kprintf(LOG_DEBUG, " type:      0x%x\n", mbr->partitions[i].type);
		kprintf(LOG_DEBUG, " start_lba: %u\n",   mbr->partitions[i].start_lba);
		kprintf(LOG_DEBUG, " num_sects: %u\n",   mbr->partitions[i].num_sects);

		disk_partition_add(device, i, mbr->partitions[i].start_lba, mbr->partitions[i].num_sects);
	}

	kfree(mbr);
	return true;
}