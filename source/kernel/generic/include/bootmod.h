/* Kiwi boot-time module loader
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
 * @brief		Boot-time module loader.
 */

#ifndef __BOOTMOD_H
#define __BOOTMOD_H

#include <types.h>

/** Structure defining a module loaded at boot-time. */
typedef struct bootmod {
	char *name;		/**< Name of the module. */
	void *addr;		/**< Address of module image in memory. */
	size_t size;		/**< Size of module image. */
	bool loaded;		/**< Whether the module has been loaded. */
} bootmod_t;

/** Boot module handler function.
 * @param mod		Module to load.
 * @return		1 if loaded, 0 if module valid but cannot be loaded
 *			yet (dependencies, etc), -1 if module not this type. */
typedef int (*bootmod_handler_t)(bootmod_t *mod);

extern bootmod_t *bootmod_array;
extern size_t bootmod_count;

extern bootmod_t *bootmod_lookup(const char *name);
extern void bootmod_handler_register(bootmod_handler_t handler);
extern void bootmod_load(void);

#endif /* __BOOTMOD_H */
