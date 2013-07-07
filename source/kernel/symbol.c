/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Kernel symbol manager.
 */

#include <lib/radix_tree.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <symbol.h>

extern symbol_table_t kernel_symtab;

/** Tree of name to symbol mappings. */
static radix_tree_t *symbol_tree = NULL;

/** List of all symbol tables (protected by module_lock). */
static LIST_DECLARE(symbol_tables);

/** Insert an entry into the symbol tree.
 * @param symbol	Symbol to insert. */
static void symbol_tree_insert(symbol_t *symbol) {
	list_t *list;

	if(!(list = radix_tree_lookup(symbol_tree, symbol->name))) {
		list = kmalloc(sizeof(list_t), MM_KERNEL);
		list_init(list);
		radix_tree_insert(symbol_tree, symbol->name, list);
	}

	list_init(&symbol->header);
	list_append(list, &symbol->header);
}

/** Remove an entry from the symbol tree.
 * @param symbol	Symbol to remove. */
static void symbol_tree_remove(symbol_t *symbol) {
	list_t *list;

	if(list_empty(&symbol->header)) {
		return;
	} else if(!(list = radix_tree_lookup(symbol_tree, symbol->name))) {
		fatal("Symbol in list but list not found");
	}

	list_remove(&symbol->header);
	if(list_empty(list))
		radix_tree_remove(symbol_tree, symbol->name, kfree);
}

/** Comparison function for symbol sorting.
 * @param _sym1		First symbol.
 * @param _sym2		Second symbol.
 * @return		Address difference. */
static int symbol_table_qsort_compare(const void *_sym1, const void *_sym2) {
	symbol_t *sym1 = (symbol_t *)_sym1, *sym2 = (symbol_t *)_sym2;
	return sym1->addr - sym2->addr;
}

/** Initialize a symbol table.
 * @param table		Table to initialize. */
void symbol_table_init(symbol_table_t *table) {
	list_init(&table->header);
	table->symbols = NULL;
	table->count = 0;
}

/** Destroy a symbol table.
 * @param table		Table to destroy. */
void symbol_table_destroy(symbol_table_t *table) {
	size_t i;

	if(table->symbols) {
		/* Only remove the symbols from the tree if the table has been
		 * published. */
		if(!list_empty(&table->header)) {
			for(i = 0; i < table->count; i++)
				symbol_tree_remove(&table->symbols[i]);
		}

		kfree(table->symbols);
	}

	list_remove(&table->header);
}

/** Insert a symbol into a symbol table.
 * @param table		Table to insert into.
 * @param name		Name of symbol.
 * @param addr		Address symbol maps to.
 * @param size		Size of the symbol.
 * @param global	Whether the symbol is a global symbol.
 * @param exported	Whether the symbol is exported to kernel modules. */
void symbol_table_insert(symbol_table_t *table, const char *name, ptr_t addr,
	size_t size, bool global, bool exported)
{
	size_t i;

	/* Resize the symbol table so we can fit the symbol in. */
	i = table->count++;
	table->symbols = krealloc(table->symbols, table->count * sizeof(symbol_t), MM_KERNEL);

	/* Fill in the symbol information. */
	table->symbols[i].addr = addr;
	table->symbols[i].size = size;
	table->symbols[i].name = name;
	table->symbols[i].global = global;
	table->symbols[i].exported = exported;
}

/** Publish a symbol table.
 * @param table		Table to publish. */
void symbol_table_publish(symbol_table_t *table) {
	size_t i;

	if(!table->count || !table->symbols)
		fatal("Publishing empty symbol table");

	/* Sort the table into symbol order. */
	qsort(table->symbols, table->count, sizeof(symbol_t), symbol_table_qsort_compare);

	/* Place all of the symbols into the global symbol tree. */
	for(i = 0; i < table->count; i++)
		symbol_tree_insert(&table->symbols[i]);

	/* Add the table to the table list. */
	list_append(&symbol_tables, &table->header);
}

/**
 * Look up symbol by address.
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

	if(addr < table->symbols[0].addr)
		return NULL;

	for(i = 0; i < table->count; i++) {
		end = table->symbols[i].addr + table->symbols[i].size;

		if(addr >= table->symbols[i].addr && addr < end) {
			if(offp != NULL)
				*offp = (size_t)(addr - table->symbols[i].addr);

			return &table->symbols[i];
		}
	}

	if(offp != NULL)
		*offp = 0;

	return NULL;
}

/**
 * Look up symbol by name.
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
symbol_t *symbol_table_lookup_name(symbol_table_t *table, const char *name,
	bool global, bool exported)
{
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

/**
 * Look up symbol by address.
 *
 * Looks for the symbol corresponding to an address in all symbol tables, and
 * gets the offset of the address in the symbol.
 *
 * @param addr		Address to lookup.
 * @param offp		Where to store symbol offset (can be NULL).
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_lookup_addr(ptr_t addr, size_t *offp) {
	symbol_table_t *table;
	symbol_t *sym;

	if(!list_empty(&symbol_tables)) {
		LIST_FOREACH(&symbol_tables, iter) {
			table = list_entry(iter, symbol_table_t, header);

			sym = symbol_table_lookup_addr(table, addr, offp);
			if(sym)
				return sym;
		}

		return NULL;
	} else {
		return symbol_table_lookup_addr(&kernel_symtab, addr, offp);
	}
}

/**
 * Look up symbol by name.
 *
 * Looks for a symbol with the name specified in all symbol tables. If
 * specified, will only look for global and/or exported symbols.
 *
 * @param name		Name to lookup.
 * @param global	Whether to only look up global symbols.
 * @param exported	Whether to only look up exported symbols.
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *symbol_lookup_name(const char *name, bool global, bool exported) {
	symbol_t *sym;
	list_t *list;

	if(symbol_tree) {
		if(!(list = radix_tree_lookup(symbol_tree, name)))
			return NULL;

		LIST_FOREACH(list, iter) {
			sym = list_entry(iter, symbol_t, header);

			if(global && !sym->global) {
				continue;
			} else if(exported && !sym->exported) {
				continue;
			} else {
				return sym;
			}
		}

		return NULL;
	} else {
		return symbol_table_lookup_name(&kernel_symtab, name, global, exported);
	}
}

/** Initialize the kernel symbol manager. */
__init_text void symbol_init(void) {
	symbol_tree = kmalloc(sizeof(radix_tree_t), MM_BOOT);
	radix_tree_init(symbol_tree);

	/* Publish the kernel symbol table in the tree. */
	list_init(&kernel_symtab.header);
	symbol_table_publish(&kernel_symtab);
}
