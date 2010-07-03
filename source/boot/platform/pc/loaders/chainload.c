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
 * @brief		PC chainload loader type.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <boot/loader.h>

#include <platform/bios.h>
#include <platform/boot.h>

#include <fatal.h>

extern void chain_loader_enter(uint8_t id, ptr_t part) __noreturn;

/** Details of where to load stuff to. */
#define CHAINLOAD_ADDR		0x7C00
#define CHAINLOAD_SIZE		512
#define PARTITION_TABLE_ADDR	0x7BE
#define PARTITION_TABLE_OFFSET	446
#define PARTITION_TABLE_SIZE	64

/** Load a chainload entry.
 * @note		Assumes the disk has an MSDOS partition table.
 * @param env		Environment for the OS. */
static void __noreturn chain_loader_load(environ_t *env) {
	ptr_t part_addr = 0;
	bios_regs_t regs;
	disk_t *parent;
	uint8_t id;

	/* Get the ID of the disk we're booting from. */
	id = bios_disk_id(current_disk);
	dprintf("loader: chainloading from device %s (id: 0x%x)\n", current_disk->name, id);

	/* Load the boot sector. */
	if(!disk_read(current_disk, (void *)CHAINLOAD_ADDR, CHAINLOAD_SIZE, 0)) {
		fatal("Could not read boot sector");
	}

	/* If booting a partition, we must give partition information to it. */
	if((parent = disk_parent(current_disk)) != current_disk) {
		if(!disk_read(parent, (void *)PARTITION_TABLE_ADDR, PARTITION_TABLE_SIZE,
		              PARTITION_TABLE_OFFSET)) {
			fatal("Could not read partition table");
		}

		part_addr = PARTITION_TABLE_ADDR + (current_disk->id << 4);
	}

	/* Try to disable the A20 line. */
	bios_regs_init(&regs);
	regs.eax = 0x2400;
	bios_interrupt(0x15, &regs);
	if(regs.eflags & X86_FLAGS_CF || regs.eax != 0) {
		out8(0x92, in8(0x92) & ~(1<<1));
	}

	/* Restore the console to a decent state. */
	bios_regs_init(&regs);
	regs.eax = 0x0500;
	bios_interrupt(0x10, &regs);
	bios_regs_init(&regs);
	regs.eax = 0x0200;
	bios_interrupt(0x10, &regs);

	/* Drop to real mode and jump to the boot sector. */
	chain_loader_enter(id, part_addr);
}

/** Chainload loader type. */
static loader_type_t chain_loader_type = {
	.load = chain_loader_load,
};

/** Chainload another boot sector.
 * @param args		Arguments for the command.
 * @param env		Environment to use.
 * @return		Whether successful. */
bool config_cmd_chainload(value_list_t *args, environ_t *env) {
	if(args->count != 0) {
		dprintf("config: chainload: invalid arguments\n");
		return false;
	}

	loader_type_set(env, &chain_loader_type);
	return true;
}
