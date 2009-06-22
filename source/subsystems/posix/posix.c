/* Kiwi POSIX subsystem kernel-mode component
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
 * @brief		POSIX subsystem kernel-mode component.
 */

#include <console/kprintf.h>

#include <lib/utility.h>

#include <loader/elf.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <fatal.h>
#include <module.h>

/** Print a message to the console.
 * @param addr		Pointer to string to write.
 * @return		Always returns 0. */
static int posix_message(const char *addr) {
	char *str;
	int ret;

	ret = strdup_from_user(addr, MM_SLEEP, &str);
	if(ret != 0) {
		return ret;
	}

	kprintf(LOG_NORMAL, "posix_print(%p): %s\n", addr, str);
	kfree(str);
	return 0;
}

/** POSIX system call list. */
static syscall_handler_t posix_syscalls[] = {
	(syscall_handler_t)posix_message,
};

/** POSIX ELF ABI definition structure. */
static loader_elf_abi_t posix_elf_abi = {
	.string = "POSIX",
	.num = ELFOSABI_NONE,
	.subsystem = {
		.name = "POSIX",
		.syscalls = posix_syscalls,
		.syscall_count = ARRAYSZ(posix_syscalls),
	},
};

/** POSIX module initialization function.
 * @return		0 on success, negative error code on failure. */
static int posix_init(void) {
	/* Register the ELF ABI that we use (SVR4). */
	return loader_elf_abi_register(&posix_elf_abi);
}

/** POSIX module unload function.
 * @return		0 on success, negative error code on failure. */
static int posix_unload(void) {
	loader_elf_abi_unregister(&posix_elf_abi);
	return 0;
}

MODULE_NAME("posix");
MODULE_DESC("POSIX subsystem kernel-mode component.");
MODULE_FUNCS(posix_init, posix_unload);
MODULE_DEPS("loader", "vfs");
