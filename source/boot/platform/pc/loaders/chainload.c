/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		PC chainload loader type.
 */

#include <arch/io.h>
#include <loader.h>

#include "../bios.h"

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
		boot_error("Could not read boot sector");
	}

	/* If booting a partition, we must give partition information to it. */
	if((parent = disk_parent(current_disk)) != current_disk) {
		if(!disk_read(parent, (void *)PARTITION_TABLE_ADDR, PARTITION_TABLE_SIZE,
		              PARTITION_TABLE_OFFSET)) {
			boot_error("Could not read partition table");
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
