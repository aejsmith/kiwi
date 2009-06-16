/* Kiwi ELF ABI type manager
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
 * @brief		ELF ABI type manager.
 */

#ifndef __LOADER_ELF_H
#define __LOADER_ELF_H

#include <proc/subsystem.h>

#include <types/list.h>

#include <elf.h>

/** Structure defining an ELF ABI type. */
typedef struct loader_elf_abi {
	list_t header;			/**< Link to ELF ABI type list. */

	const char *string;		/**< ABI type name to look for. */
	int num;			/**< EI_OSABI value to fall back on if no ABI note. */
	subsystem_t *subsystem;		/**< Subsystem to use for this ABI type. */
} loader_elf_abi_t;

extern int loader_elf_abi_register(loader_elf_abi_t *abi);
extern void loader_elf_abi_unregister(loader_elf_abi_t *abi);

#endif /* __LOADER_ELF_H */
