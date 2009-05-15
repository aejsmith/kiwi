/* Kiwi kernel symbol manager
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Kernel symbol manager.
 */

#ifndef __KSYM_H
#define __KSYM_H

#include <types.h>

/** Information about a symbol in the kernel or a module. */
typedef struct ksym {
	ptr_t addr;		/**< Address that the symbol points to. */
	size_t size;		/**< Size of symbol. */
	const char *name;	/**< Name of the symbol. */
	bool global;		/**< Whether the symbol is global. */
	bool exported;		/**< Whether the symbol has been exported for modules to link to. */
} ksym_t;

/** Table of symbols. */
typedef struct ksym_table {
	ksym_t *symbols;	/**< Array of symbols. */
	size_t count;		/**< Number of symbols in the table. */
} ksym_table_t;

/** Kernel symbol table. */
extern ksym_table_t kernel_symtab;

extern ksym_t *ksym_lookup_addr(ksym_table_t *table, ptr_t addr, ptr_t *off);
extern ksym_t *ksym_lookup_name(ksym_table_t *table, const char *name, bool global, bool exported);

#endif /* __KSYM_H */
