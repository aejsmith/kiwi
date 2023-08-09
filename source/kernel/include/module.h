/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Kernel module loader.
 */

#pragma once

#include <kernel/module.h>

#include <lib/list.h>

#include <elf.h>
#include <object.h>

/** Module information section name. */
#define MODULE_INFO_SECTION ".module_info"

/** Module initialization function type. */
typedef status_t (*module_init_t)(void);

/** Module unload function type. */
typedef status_t (*module_unload_t)(void);

/** State of a module. */
enum {
    MODULE_STATE_LOADING,           /**< Module is being loaded. */
    MODULE_STATE_READY,             /**< Module is initialized and ready for use. */
    MODULE_STATE_UNLOADING,         /**< Module is being unloaded. */
};

/**
 * Bits in module_t::state.
 * 0-29  = Reference count.
 * 30-31 = State.
 */
#define MODULE_STATE_MASK   0xc0000000
#define MODULE_STATE_SHIFT  30
#define MODULE_COUNT_MASK   0x3fffffff

/** Structure defining a kernel module. */
typedef struct module {
    list_t header;                  /**< Link to loaded modules list. */

    atomic_uint32_t state;          /**< Combined state + reference count. */
    elf_image_t image;              /**< ELF image for the module. */

    /** Module information. */
    const char *name;               /**< Name of module. */
    const char *description;        /**< Description of the module. */
    const char **deps;              /**< Module dependencies. */
    module_init_t init;             /**< Module initialization function. */
    module_unload_t unload;         /**< Module unload function. */
} module_t;

/** Information about a symbol in an image. */
typedef struct symbol {
    ptr_t addr;                     /**< Address that the symbol points to. */
    size_t size;                    /**< Size of symbol. */
    const char *name;               /**< Name of the symbol. */
    bool global : 1;                /**< Whether the symbol is global. */
    bool exported : 1;              /**< Whether the symbol is exported. */
    elf_image_t *image;             /**< Image containing the symbol. */
} symbol_t;

/** Set the name of a module. */
#define MODULE_NAME(mname) \
    static char __used __section(MODULE_INFO_SECTION) __module_name[] = mname

/** Set the description of a module. */
#define MODULE_DESC(mdesc) \
    static char __used __section(MODULE_INFO_SECTION) __module_desc[] = mdesc

/** Set the module hook functions. */
#define MODULE_FUNCS(minit, munload) \
    static module_init_t __section(MODULE_INFO_SECTION) __used __module_init = minit; \
    static module_unload_t __section(MODULE_INFO_SECTION) __used __module_unload = munload

/** Define a module's dependencies. */
#define MODULE_DEPS(mdeps...) \
    static const char * __section(MODULE_INFO_SECTION) __used __module_deps[] = { \
        mdeps, \
        NULL \
    }

extern module_t kernel_module;

/** Get the state of a module. */
static inline uint32_t module_state(module_t *module) {
    return atomic_load_explicit(&module->state, memory_order_relaxed) >> MODULE_STATE_SHIFT;
}

/** Get the reference count of a module. */
static inline uint32_t module_count(module_t *module) {
    return atomic_load_explicit(&module->state, memory_order_relaxed) & MODULE_COUNT_MASK;
}

extern ptr_t module_mem_alloc(size_t size);
extern void module_mem_free(ptr_t base, size_t size);

extern bool module_retain(module_t *module);
extern void module_release(module_t *module);

extern module_t *module_for_addr(void *addr);
extern module_t *module_self(void);

/**
 * Finds the module containing the caller of the function that it is used
 * within, for example:
 *
 *   void foo() { module_caller(); }
 *   void bar() { foo(); }
 *
 * for a call to bar, foo will receive the module containing bar.
 *
 * Note that function inlining will cause unexpected results, so do not use
 * this in functions that might be inlined.
 */
#define module_caller() \
    module_for_addr(__builtin_return_address(0))

extern status_t module_load(const char *path, char *depbuf);
extern status_t module_unload(const char *name);

extern bool symbol_from_addr(ptr_t addr, symbol_t *symbol, size_t *_off);
extern bool symbol_lookup(const char *name, bool global, bool exported, symbol_t *symbol);

extern void module_early_init(void);
extern void module_init(void);
