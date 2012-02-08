/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel module loader.
 */

#ifndef __MODULE_H
#define __MODULE_H

#include <kernel/module.h>

#include <lib/list.h>
#include <lib/refcount.h>

#include <elf.h>
#include <object.h>
#include <symbol.h>

/** Module initialization function type. */
typedef status_t (*module_init_t)(void);

/** Module unload function type. */
typedef status_t (*module_unload_t)(void);

/** Structure defining a kernel module. */
typedef struct module {
	list_t header;			/**< Link to loaded modules list. */

	/** Internally-used information. */
	symbol_table_t symtab;		/**< Symbol table for the module. */
	refcount_t count;		/**< Count of modules depending on this module. */
	object_handle_t *handle;	/**< Handle to module file (only valid while loading). */

	/** Module information. */
	const char *name;		/**< Name of module. */
	const char *description;	/**< Description of the module. */
	const char **deps;		/**< Module dependencies. */
	module_init_t init;		/**< Module initialization function. */
	module_unload_t unload;		/**< Module unload function. */

	/** ELF loader information. */
	elf_ehdr_t ehdr;		/**< ELF executable header. */
	elf_shdr_t *shdrs;		/**< ELF section headers. */
	void *load_base;		/**< Address of allocation module is loaded to. */
	size_t load_size;		/**< Size of allocation module is loaded to. */
} module_t;

/** Set the name of a module. */
#define MODULE_NAME(mname)		\
	static char __used __section(MODULE_INFO_SECTION) __module_name[] = mname

/** Set the description of a module. */
#define MODULE_DESC(mdesc)              \
        static char __used __section(MODULE_INFO_SECTION) __module_desc[] = mdesc

/** Set the module hook functions. */
#define MODULE_FUNCS(minit, munload)    \
	static module_init_t __section(MODULE_INFO_SECTION) __used __module_init = minit; \
	static module_unload_t __section(MODULE_INFO_SECTION) __used __module_unload = munload

/** Define a module's dependencies. */
#define MODULE_DEPS(mdeps...)           \
        static const char * __section(MODULE_INFO_SECTION) __used __module_deps[] = { \
                mdeps, \
                NULL \
        }

/** Export a symbol from a module. */
#define MODULE_EXPORT(msym)             \
        static const char *__module_export_##msym \
                __section(MODULE_EXPORT_SECTION) __used = #msym

extern void *module_mem_alloc(size_t size);

extern status_t module_name(object_handle_t *handle, char *namebuf);
extern status_t module_load(object_handle_t *handle, char *depbuf);

#endif /* __MODULE_H */
