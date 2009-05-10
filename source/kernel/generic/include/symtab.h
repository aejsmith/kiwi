/* Kiwi symbol table functions
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

#ifndef __SYMTAB_H
#define __SYMTAB_H

//#include <types.h>

typedef unsigned long ptr_t;
typedef unsigned long size_t;

/** Class declaring a symbol table. */
class SymbolTable {
public:
	/** Structure defining a symbol within a symbol table. */
	struct Symbol {
		ptr_t addr;		/**< Address of the symbol. */
		ptr_t size;		/**< Size of the symbol. */
		const char *name;	/**< Name of the symbol. */
		bool global;		/**< Whether the symbol is global. */
		bool exported;		/**< Whether the symbol is exported. */
	};
private:
	Symbol *m_symbols;		/**< Array of symbols. */
	size_t m_count;			/**< Symbol count. */
};

#endif /* __SYMTAB_H */
