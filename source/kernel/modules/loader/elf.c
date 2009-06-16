/* Kiwi ELF executable loader
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
 * @brief		ELF executable loader.
 */

#include <lib/string.h>

#include <sync/mutex.h>

#include "loader_priv.h"

/** List of known ELF ABI types. */
static LIST_DECLARE(elf_abi_list);
static MUTEX_DECLARE(elf_abi_list_lock);

/** Check whether a binary is an ELF binary with a known ABI.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the binary is a known ELF. */
static bool loader_elf_check(vfs_node_t *node) {
	return true;
}

/** Load an ELF binary into an address space.
 * @param data		Binary loader data structure.
 * @return		0 on success, negative error code on failure. */
static int loader_elf_load(loader_binary_t *data) {
	return 0;
}

/** Finish binary loading, after address space is switched.
 * @param data		Binary loader data structure.
 * @return		0 on success, negative error code on failure. */
static int loader_elf_finish(loader_binary_t *data) {
	return 0;
}

/** ELF executable loader type. */
loader_type_t loader_elf_type = {
	.name = "ELF",
	.check = loader_elf_check,
	.load = loader_elf_load,
	.finish = loader_elf_finish,
};

/** Register an ELF ABI type.
 *
 * Registers an ELF ABI type with the loader. This system allows for multiple
 * subsystems based on ELF, and makes it easy to choose which subsystem to
 * run a binary on. There are two methods for matching a binary to an ABI. If
 * a binary provides a note (name Kiwi, type 1), then the note specifies the
 * name of an ABI to use. If a note is not specified, then the loader will
 * attempt to match the binary's OS/ABI field in the ELF header to an ABI.
 *
 * @param abi		ABI to register.
 *
 * @return		0 on success, negative error code on failure.
 */
int loader_elf_abi_register(loader_elf_abi_t *abi) {
	loader_elf_abi_t *exist;

	if(!abi->string || !abi->subsystem) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&elf_abi_list_lock, 0);

	/* Check if the ABI already exists. */
	LIST_FOREACH(&elf_abi_list, iter) {
		exist = list_entry(iter, loader_elf_abi_t, header);

		if(exist->string && abi->string && (strcmp(exist->string, abi->string) == 0)) {
			mutex_unlock(&elf_abi_list_lock);
			return -ERR_OBJ_EXISTS;
		} else if(exist->num != -1 && exist->num == abi->num) {
			mutex_unlock(&elf_abi_list_lock);
			return -ERR_OBJ_EXISTS;
		}
	}

	list_init(&abi->header);
	list_append(&elf_abi_list, &abi->header);

	dprintf("loader: registered ELF ABI type 0x%p(%s:%d)\n", abi, abi->string, abi->num);
	mutex_unlock(&elf_abi_list_lock);
	return 0;
}
MODULE_EXPORT(loader_elf_abi_register);

/** Remove an ELF ABI type.
 *
 * Removes an ELF ABI type from the ABI type list.
 *
 * @param abi		ABI to remove.
 */
void loader_elf_abi_unregister(loader_elf_abi_t *abi) {
	mutex_lock(&elf_abi_list_lock, 0);
	list_remove(&abi->header);
	mutex_unlock(&elf_abi_list_lock);
}
MODULE_EXPORT(loader_elf_abi_unregister);
