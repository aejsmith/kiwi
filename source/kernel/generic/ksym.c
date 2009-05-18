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

#include <lib/string.h>

#include <assert.h>
#include <ksym.h>

/** Look up symbol from address.
 *
 * Looks for the symbol corresponding to an address in a symbol table, and
 * gets the offset of the address in the symbol.
 *
 * @param table		Symbol table to look in.
 * @param addr		Address to lookup.
 * @param off		Where to store symbol offset (can be NULL).
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
ksym_t *ksym_lookup_addr(ksym_table_t *table, ptr_t addr, size_t *off) {
	ptr_t end;
	size_t i;

	assert(table);

	if(addr < table->symbols[0].addr) {
		return NULL;
	}

	for(i = 0; i < table->count; i++) {
		end = table->symbols[i].addr + table->symbols[i].size;

		if(addr >= table->symbols[i].addr && addr < end) {
			if(off != NULL) {
				*off = (size_t)(addr - table->symbols[i].addr);
			}
			return &table->symbols[i];
		}
	}

	if(off != NULL) {
		*off = 0;
	}
	return NULL;
}

/** Look up symbol from name.
 *
 * Looks for a symbol with the name specified in a symbol table. If specified,
 * will only look for global and/or exported symbols.
 *
 * @param table		Symbol table to look in.
 * @param name		Name to lookup.
 * @param global	Whether to only look up global symbols.
 * @param exported	Whether to only look up exported symbols
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
ksym_t *ksym_lookup_name(ksym_table_t *table, const char *name, bool global, bool exported) {
	size_t i;

	assert(table);
	assert(name);

	for(i = 0; i < table->count; i++) {
		if(strcmp(name, table->symbols[i].name) == 0) {
			if(global && !table->symbols[i].global) {
				continue;
			} else if(exported && !table->symbols[i].exported) {
				continue;
			}

			return &table->symbols[i];
		}
	}

	return NULL;
}
