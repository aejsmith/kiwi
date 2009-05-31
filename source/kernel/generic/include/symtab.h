/* Kiwi symbol table manager
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
 * @brief		Symbol table manager.
 */

#ifndef __SYMTAB_H
#define __SYMTAB_H

#include <types.h>

/** Information about a symbol in the kernel or a module. */
typedef struct symbol {
	ptr_t addr;		/**< Address that the symbol points to. */
	size_t size;		/**< Size of symbol. */
	const char *name;	/**< Name of the symbol. */
	bool global;		/**< Whether the symbol is global. */
	bool exported;		/**< Whether the symbol has been exported for modules to link to. */
} symbol_t;

/** Structure containing a symbol table. */
typedef struct symtab {
	symbol_t *symbols;	/**< Array of symbols. */
	size_t count;		/**< Number of symbols in the table. */
} symtab_t;

/** Kernel symbol table. */
extern symtab_t kernel_symtab;

extern symbol_t *symtab_lookup_addr(symtab_t *table, ptr_t addr, size_t *offp);
extern symbol_t *symtab_lookup_name(symtab_t *table, const char *name, bool global, bool exported);

extern void symtab_init(symtab_t *table);
extern void symtab_insert(symtab_t *table, const char *name, ptr_t addr, size_t size,
                          bool global, bool exported);

#endif /* __SYMTAB_H */
