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

#include <lib/qsort.h>
#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <symbol.h>

/** Comparison function for symbol sorting.
 * @param _sym1		First symbol.
 * @param _sym2		Second symbol.
 * @return		Address difference. */
static int symbol_table_qsort_compare(const void *_sym1, const void *_sym2) {
	symbol_t *sym1 = (symbol_t *)_sym1, *sym2 = (symbol_t *)_sym2;
	return sym1->addr - sym2->addr;
}

/** Look up symbol from address.
 *
 * Looks for the symbol corresponding to an address in a symbol table, and
 * gets the offset of the address in the symbol.
 *
 * @param table		Symbol table to look in.
 * @param addr		Address to lookup.
 * @param offp		Where to store symbol offset (can be NULL).
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_table_lookup_addr(symbol_table_t *table, ptr_t addr, size_t *offp) {
	ptr_t end;
	size_t i;

	assert(table);

	if(addr < table->symbols[0].addr) {
		return NULL;
	}

	for(i = 0; i < table->count; i++) {
		end = table->symbols[i].addr + table->symbols[i].size;

		if(addr >= table->symbols[i].addr && addr < end) {
			if(offp != NULL) {
				*offp = (size_t)(addr - table->symbols[i].addr);
			}
			return &table->symbols[i];
		}
	}

	if(offp != NULL) {
		*offp = 0;
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
 * @param exported	Whether to only look up exported symbols.
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_table_lookup_name(symbol_table_t *table, const char *name, bool global, bool exported) {
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

/** Initialize a symbol table.
 *
 * Initializes a symbol table structure.
 *
 * @param table		Table to initialize.
 */
void symbol_table_init(symbol_table_t *table) {
	table->symbols = NULL;
	table->count = 0;
}

/** Insert a symbol into a symbol table.
 *
 * Inserts a symbol into a symbol table and then sorts the symbol table to
 * keep it ordered by address.
 *
 * @param table		Table to insert into.
 * @param name		Name of symbol.
 * @param addr		Address symbol maps to.
 * @param size		Size of the symbol.
 * @param global	Whether the symbol is a global symbol.
 * @param exported	Whether the symbol is exported to kernel modules.
 */
void symbol_table_insert(symbol_table_t *table, const char *name, ptr_t addr, size_t size,
                         bool global, bool exported) {
	size_t i = table->count;

	/* Resize the symbol table so we can fit the symbol in. */
	table->count++;
	table->symbols = krealloc(table->symbols, table->count * sizeof(symbol_t), MM_SLEEP);

	/* Fill in the symbol information. */
	table->symbols[i].name = name;
	table->symbols[i].addr = addr;
	table->symbols[i].size = size;
	table->symbols[i].global = global;
	table->symbols[i].exported = exported;

	/* Re-sort the table. */
	qsort(table->symbols, table->count, sizeof(symbol_t), symbol_table_qsort_compare);
}

/** Look up symbol from address.
 *
 * Looks for the symbol corresponding to an address in the kernel symbol table
 * and all module symbol tables, and gets the offset of the address in the
 * symbol.
 *
 * @param addr		Address to lookup.
 * @param offp		Where to store symbol offset (can be NULL).
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_lookup_addr(ptr_t addr, size_t *offp) {
	symbol_t *sym;

	sym = symbol_table_lookup_addr(&kernel_symtab, addr, offp);
	if(sym) {
		return sym;
	}

	return NULL;
}

/** Look up symbol from name.
 *
 * Looks for a symbol with the name specified in the kernel symbol table and
 * all module symbol tables. If specified, will only look for global and/or
 * exported symbols.
 *
 * @param name		Name to lookup.
 * @param global	Whether to only look up global symbols.
 * @param exported	Whether to only look up exported symbols.
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_lookup_name(const char *name, bool global, bool exported) {
	symbol_t *sym;

	sym = symbol_table_lookup_name(&kernel_symtab, name, global, exported);
	if(sym) {
		return sym;
	}

	return NULL;
}
