/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		BIOS disk device functions.
 */

#include <boot/console.h>
#include <boot/disk.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <platform/boot.h>

#include <assert.h>

#include "bios.h"
#include "multiboot.h"
#include "pxe.h"

/** Drive parameters structure. We only care about the EDD 1.x fields. */
typedef struct drive_parameters {
	uint16_t size;
	uint16_t flags;
	uint32_t cylinders;
	uint32_t heads;
	uint32_t spt;
	uint64_t sector_count;
	uint16_t sector_size;
} __packed drive_parameters_t;

/** Disk address packet structure. */
typedef struct disk_address_packet {
	uint8_t size;
	uint8_t reserved1;
	uint16_t block_count;
	uint16_t buffer_offset;
	uint16_t buffer_segment;
	uint64_t start_lba;
} __packed disk_address_packet_t;

/** Bootable CD-ROM Specification Packet. */
typedef struct specification_packet {
        uint8_t size;
        uint8_t media_type;
        uint8_t drive_number;
        uint8_t controller_num;
        uint32_t image_lba;
        uint16_t device_spec;
} __packed specification_packet_t;

/** Structure used to store details of a BIOS disk. */
typedef struct bios_disk {
	uint8_t id;			/**< BIOS device ID. */
} bios_disk_t;

/** Maximum number of blocks per transfer. */
#define BLOCKS_PER_TRANSFER(disk)	((BIOS_MEM_SIZE / disk->block_size) - 1)

extern uint8_t boot_device_id;
extern uint64_t boot_part_offset;

/** Check if a partition is the boot partition.
 * @param disk		Disk the partition resides on.
 * @param id		ID of partition.
 * @param lba		Block that the partition starts at.
 * @return		Whether partition is a boot partition. */
static bool bios_disk_is_boot_partition(disk_t *disk, uint8_t id, uint64_t lba) {
	if(multiboot_magic == MB_LOADER_MAGIC && (uint64_t)id == boot_part_offset) {
		return true;
	} else if(lba == boot_part_offset) {
		return true;
	} else {
		return false;
	}
}

/** Read blocks from a BIOS disk device.
 * @param disk		Disk to read from.
 * @param buf		Buffer to read into.
 * @param lba		Starting block number.
 * @param count		Number of blocks to read.
 * @return		Whether the read succeeded. */
static bool bios_disk_read(disk_t *disk, void *buf, uint64_t lba, size_t count) {
	disk_address_packet_t *dap = (disk_address_packet_t *)BIOS_MEM_BASE;
	void *dest = (void *)(BIOS_MEM_BASE + disk->block_size);
	bios_disk_t *data = disk->data;
	size_t i, num, transfers;
	bios_regs_t regs;

	/* Have to split large transfers up as we have limited space to
	 * transfer to. */
	transfers = ROUND_UP(count, BLOCKS_PER_TRANSFER(disk)) / BLOCKS_PER_TRANSFER(disk);
	for(i = 0; i < transfers; i++, buf += disk->block_size * num, count -= num) {
		num = MIN(count, BLOCKS_PER_TRANSFER(disk));

		/* Fill in a disk address packet for the transfer. */
		dap->size = sizeof(disk_address_packet_t);
		dap->reserved1 = 0;
		dap->block_count = num;
		dap->buffer_offset = (ptr_t)dest;
		dap->buffer_segment = 0;
		dap->start_lba = lba + (i * BLOCKS_PER_TRANSFER(disk));

		/* Perform the transfer. */
		bios_regs_init(&regs);
		regs.eax = 0x4200;
		regs.edx = data->id;
		regs.esi = BIOS_MEM_BASE;
		bios_interrupt(0x13, &regs);
		if(regs.eflags & X86_FLAGS_CF) {
			return false;
		}

		/* Copy the transferred blocks to the buffer. */
		memcpy(buf, dest, disk->block_size * num);
	}

	return true;
}

/** Operations for a BIOS disk device. */
static disk_ops_t bios_disk_ops = {
	.is_boot_partition = bios_disk_is_boot_partition,
	.read = bios_disk_read,
};

/** Get the number of disks in the system.
 * @return		Number of BIOS hard disks. */
static uint8_t platform_disk_count(void) {
	bios_regs_t regs;

	/* Use the Get Drive Parameters call. */
	bios_regs_init(&regs);
	regs.eax = 0x800;
	regs.edx = 0x80;
	bios_interrupt(0x13, &regs);
	return (regs.eflags & X86_FLAGS_CF) ? 0 : (regs.edx & 0xFF);
}

/** Check if booted from CD.
 * @return		Whether booted from CD. */
static bool platform_booted_from_cd(void) {
	specification_packet_t *packet = (specification_packet_t *)BIOS_MEM_BASE;
	bios_regs_t regs;

	/* Use the bootable CD-ROM status function. */
	bios_regs_init(&regs);
	regs.eax = 0x4B01;
	regs.edx = boot_device_id;
	regs.esi = BIOS_MEM_BASE;
	bios_interrupt(0x13, &regs);
	return (!(regs.eflags & X86_FLAGS_CF) && packet->drive_number == boot_device_id);
}

/** Add the disk with the specified ID.
 * @param id		ID of the device. */
static void platform_disk_add(uint8_t id) {
	drive_parameters_t *params = (drive_parameters_t *)BIOS_MEM_BASE;
	bios_disk_t *data;
	bios_regs_t regs;
	char name[6];

	/* Create a data structure for the device. */
	data = kmalloc(sizeof(bios_disk_t));
	data->id = id;

	/* Probe for information on the device. A big "FUCK YOU" to Intel and
	 * AMI is required here. When booted from a CD, the INT 13 Extensions
	 * Installation Check/Get Drive Parameters functions return an error
	 * on Intel/AMI BIOSes, yet the Extended Read function still works.
	 * Work around this by forcing use of extensions when booted from CD. */
	if(id == boot_device_id && platform_booted_from_cd()) {
		disk_add(kstrdup("cd0"), 2048, ~0LL, &bios_disk_ops, data, NULL, true);
		dprintf("disk: detected boot CD cd0 (id: 0x%x)\n", id);
	} else {
		bios_regs_init(&regs);
		regs.eax = 0x4100;
		regs.ebx = 0x55AA;
		regs.edx = id;
		bios_interrupt(0x13, &regs);
		if(regs.eflags & X86_FLAGS_CF || (regs.ebx & 0xFFFF) != 0xAA55 || !(regs.ecx & (1<<0))) {
			dprintf("disk: device 0x%x does not support extensions, ignoring\n", id);
			kfree(data);
			return;
		}

		/* Get drive parameters. According to RBIL, some Phoenix BIOSes
		 * fail to correctly handle the function if the flags word is
		 * not 0. Clear the entire structure to be on the safe side. */
		memset(params, 0, sizeof(drive_parameters_t));
		params->size = sizeof(drive_parameters_t);
		bios_regs_init(&regs);
		regs.eax = 0x4800;
		regs.edx = id;
		regs.esi = BIOS_MEM_BASE;
		bios_interrupt(0x13, &regs);
		if(regs.eflags & X86_FLAGS_CF || !params->sector_count || !params->sector_size) {
			dprintf("disk: failed to obtain device parameters for device 0x%x\n", id);
			return;
		}

		/* Register the disk with the disk manager. */
		sprintf(name, "hd%u", id - 0x80);
		disk_add(kstrdup(name), params->sector_size, params->sector_count,
		         &bios_disk_ops, data, NULL, id == boot_device_id);
		dprintf("disk: detected device %s (id: 0x%x, sector_size: %u, sector_count: %zu)\n",
		        name, id, params->sector_size, params->sector_count);
	}
}

/** Detect all disks in the system. */
void platform_disk_detect(void) {
	uint8_t count, id;

	/* If booted from Multiboot, boot_part_offset stores the ID of the
	 * boot partition rather than its offset. */
	if(multiboot_magic == MB_LOADER_MAGIC) {
		dprintf("disk: boot device ID is 0x%x, partition ID is %" PRIu64 "\n",
		        boot_device_id, boot_part_offset);
	} else {
		dprintf("disk: boot device ID is 0x%x, partition offset is 0x%" PRIx64 "\n",
		        boot_device_id, boot_part_offset);
	}

	/* Probe all hard disks. */
	count = platform_disk_count();
	for(id = 0x80; id < count + 0x80; id++) {
		/* If this is the boot device, ignore it - it will be added
		 * after the loop is completed. This is done because this loop
		 * only probes hard disks, so in order to support CD's, etc,
		 * we have to add the boot disk separately. */
		if(id == boot_device_id) {
			continue;
		}

		platform_disk_add(id);
	}

	/* If not booted from PXE, add the boot device. */
	if(!pxe_detect()) {
		platform_disk_add(boot_device_id);
	}
}

/** Get the ID of a disk.
 * @param disk		Disk to get ID of (can be a partition). */
uint8_t bios_disk_id(disk_t *disk) {
	bios_disk_t *data;

	disk = disk_parent(disk);
	assert(disk->ops == &bios_disk_ops);
	data = disk->data;
	return data->id;
}
