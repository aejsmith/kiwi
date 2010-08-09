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
 * @brief		MSDOS partition table scanner.
 */

#include <lib/utility.h>

#include <mm/malloc.h>

#include <console.h>
#include <status.h>

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
} __packed msdos_part_t;

/** MS-DOS partition table. */
typedef struct msdos_mbr {
	uint8_t bootcode[446];
	msdos_part_t partitions[4];
	uint16_t signature;
} __packed msdos_mbr_t;

/** Probe a disk for an MSDOS partition table.
 * @param device	Device to scan.
 * @return		Whether an MSDOS partition table was found. */
bool partition_probe_msdos(disk_device_t *device) {
	msdos_mbr_t *mbr = kmalloc(sizeof(msdos_mbr_t), MM_SLEEP);
	msdos_part_t *part;
	size_t bytes, i;
	status_t ret;

	ret = disk_device_read(device, mbr, sizeof(msdos_mbr_t), 0, &bytes);
	if(ret != STATUS_SUCCESS || bytes != sizeof(msdos_mbr_t)) {
		kprintf(LOG_WARN, "disk: could not read MSDOS MBR from %p (%d)\n",
			device, ret);
		kfree(mbr);
		return false;
	} else if(mbr->signature != MSDOS_SIGNATURE) {
		kfree(mbr);
		return false;
	}

	/* Loop through all partitions in the table. */
	for(i = 0; i < ARRAYSZ(mbr->partitions); i++) {
		part = &mbr->partitions[i];
		if(part->type == 0 || (part->bootable != 0 && part->bootable != 0x80)) {
			continue;
		} else if(part->start_lba >= device->blocks) {
			continue;
		} else if(part->start_lba + part->num_sects > device->blocks) {
			continue;
		}

		kprintf(LOG_NORMAL, "disk: found MSDOS partition %d on disk %d:\n", i, device->id);
		kprintf(LOG_NORMAL, " type:      0x%x\n", part->type);
		kprintf(LOG_NORMAL, " start_lba: %u\n", part->start_lba);
		kprintf(LOG_NORMAL, " num_sects: %u\n", part->num_sects);

		partition_add(device, i, part->start_lba, part->num_sects);
	}

	kfree(mbr);
	return true;
}
