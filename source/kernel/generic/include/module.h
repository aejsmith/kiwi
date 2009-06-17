/* Kiwi kernel module loader
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
 * @brief		Kernel module loader.
 */

#ifndef __MODULE_H
#define __MODULE_H

#include <types/list.h>
#include <types/refcount.h>

#include <elf.h>
#include <symbol.h>

/** Filename extension of kernel modules. */
#define MODULE_EXTENSION	".mod"

/** Maximum length of a module name. */
#define MODULE_NAME_MAX		16

/** Module initialization function type. */
typedef int (*module_init_t)(void);

/** Module unload function type. */
typedef int (*module_unload_t)(void);

/** Structure defining a kernel module. */
typedef struct module {
	list_t header;			/**< Link to loaded modules list. */

	/** Internally-used information. */
	symbol_table_t symtab;		/**< Symbol table for the module. */
	refcount_t count;		/**< Count of modules depending on this module. */

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

/** Get a section header from a module structure. */
#define MODULE_ELF_SECT(m, i)		\
	((elf_shdr_t *)(((ptr_t)((m)->shdrs)) + (m)->ehdr.e_shentsize * (i)))

/** Set the name of a module. */
#define MODULE_NAME(mname)		\
	static const char * __used __module_name = mname

/** Set the description of a module. */
#define MODULE_DESC(mdesc)              \
        static const char * __used __module_desc = mdesc

/** Set the module hook functions. */
#define MODULE_FUNCS(minit, munload)    \
	static module_init_t __used __module_init = minit; \
	static module_unload_t __used __module_unload = munload

/** Define a module's dependencies. */
#define MODULE_DEPS(mdeps...)           \
        static const char * __used __module_deps[] = { \
                mdeps, \
                NULL \
        }

/** Export a symbol from a module. */
#define MODULE_EXPORT(msym)             \
        static const char *__module_export_##msym \
                __section(".modexports") __used = #msym

extern bool module_check(void *image, size_t size);
extern int module_load(void *image, size_t size, char *depbuf);

extern symbol_t *module_symbol_lookup_addr(ptr_t addr, size_t *offp);
extern symbol_t *module_symbol_lookup_name(const char *name, bool global, bool exported);

extern int kdbg_cmd_modules(int argc, char **argv);

#endif /* __MODULE_H */
