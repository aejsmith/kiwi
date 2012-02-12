/*
 * Copyright (C) 2009 Alex Smith
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

#include <lib/utility.h>

#include <mm/malloc.h>

#include <kernel.h>
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
	msdos_mbr_t *mbr = kmalloc(sizeof(msdos_mbr_t), MM_WAIT);
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
	for(i = 0; i < ARRAY_SIZE(mbr->partitions); i++) {
		part = &mbr->partitions[i];
		if(part->type == 0 || (part->bootable != 0 && part->bootable != 0x80)) {
			continue;
		} else if(part->start_lba >= device->blocks) {
			continue;
		} else if(part->start_lba + part->num_sects > device->blocks) {
			continue;
		}

		kprintf(LOG_NOTICE, "disk: found MSDOS partition %d on disk %d:\n", i, device->id);
		kprintf(LOG_NOTICE, " type:      0x%x\n", part->type);
		kprintf(LOG_NOTICE, " start_lba: %u\n", part->start_lba);
		kprintf(LOG_NOTICE, " num_sects: %u\n", part->num_sects);

		partition_add(device, i, part->start_lba, part->num_sects);
	}

	kfree(mbr);
	return true;
}
