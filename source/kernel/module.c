/*
 * Copyright (C) 2009-2022 Alex Smith
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

#include <io/fs.h>
#include <io/memory_file.h>

#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/safe.h>

#include <sync/mutex.h>

#include <assert.h>
#include <kdb.h>
#include <kernel.h>
#include <kboot.h>
#include <module.h>
#include <status.h>

/** Define to enable debug output from the module loader. */
//#define DEBUG_MODULE

#ifdef DEBUG_MODULE
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** List of loaded modules. */
static LIST_DEFINE(module_list);
static MUTEX_DEFINE(module_lock, 0);

/** Kernel module structure. */
module_t kernel_module;

static LIST_DEFINE(boot_module_list);

#ifdef KERNEL_MODULE_BASE

/** Module memory allocation space. */
static ptr_t next_module_addr       = KERNEL_MODULE_BASE;
static size_t remaining_module_size = KERNEL_MODULE_SIZE;

/** Allocate memory suitable to hold a kernel module.
 * @param size          Size of the allocation.
 * @return              Address allocated or 0 if no available memory. */
ptr_t module_mem_alloc(size_t size) {
    size = round_up(size, PAGE_SIZE);
    if (size > remaining_module_size)
        return 0;

    ptr_t addr = next_module_addr;

    mmu_context_lock(&kernel_mmu_context);

    size_t i;
    for (i = 0; i < size; i += PAGE_SIZE) {
        page_t *page = page_alloc(MM_USER);
        if (!page) {
            kprintf(LOG_DEBUG, "module: unable to allocate pages to back allocation\n");
            goto fail;
        }

        status_t ret = mmu_context_map(
            &kernel_mmu_context, addr + i, page->addr,
            MMU_ACCESS_RW | MMU_ACCESS_EXECUTE, MM_USER);
        if (ret != STATUS_SUCCESS) {
            kprintf(LOG_DEBUG, "module: failed to map page 0x%" PRIxPHYS " to %p\n", page->addr, addr + i);
            page_free(page);
            goto fail;
        }
    }

    mmu_context_unlock(&kernel_mmu_context);

    next_module_addr      += size;
    remaining_module_size -= size;

    return addr;

fail:
    /* Go back and reverse what we have done. */
    for (; i; i -= PAGE_SIZE) {
        page_t *page;
        mmu_context_unmap(&kernel_mmu_context, addr + (i - PAGE_SIZE), true, &page);
        page_free(page);
    }

    mmu_context_unlock(&kernel_mmu_context);
    return 0;
}

/** Free memory holding a module.
 * @param base          Base of the allocation.
 * @param size          Size of the allocation. */
void module_mem_free(ptr_t base, size_t size) {
    /* TODO */
}

#else /* KERNEL_MODULE_BASE */

/** Allocate memory suitable to hold a kernel module.
 * @param size          Size of the allocation.
 * @return              Address allocated or 0 if no available memory. */
ptr_t module_mem_alloc(size_t size) {
    return kmem_alloc_etc(
        round_up(size, PAGE_SIZE),
        MMU_ACCESS_RW | MMU_ACCESS_EXECUTE, MM_NOWAIT);
}

/** Free memory holding a module.
 * @param base          Base of the allocation.
 * @param size          Size of the allocation. */
void module_mem_free(ptr_t base, size_t size) {
    kmem_free((void *)base, round_up(size, PAGE_SIZE));
}

#endif /* KERNEL_MODULE_BASE */

/**
 * Try to increase the reference count of a module. Holding a reference to a
 * module prevents it from being unloaded. This will only succeed if the module
 * is currently in the ready state.
 *
 * @return              Whether a reference could be added.
 */
bool module_retain(module_t *module) {
    uint32_t current = atomic_load(&module->state);

    while (true) {
        if ((current >> MODULE_STATE_SHIFT) != MODULE_STATE_READY)
            return false;

        uint32_t new = current + 1;

        if (atomic_compare_exchange_strong(&module->state, &current, new))
            return true;
    }
}

/** Release a reference to a module. */
void module_release(module_t *module) {
    assert(module_count(module) > 0);
    atomic_fetch_sub(&module->state, 1);
}

/** Find the module containing a given address.
 * @param addr          Address to find. Must be a kernel code address.
 * @return              Module containing address (always returns a valid
 *                      module - returns kernel_module if not in an actual
 *                      module). */
module_t *module_for_addr(void *addr) {
    ptr_t ptr = (ptr_t)addr;

    list_foreach(&module_list, iter) {
        module_t *module = list_entry(iter, module_t, header);

        if (module != &kernel_module &&
            ptr >= module->image.load_base &&
            ptr < (module->image.load_base + module->image.load_size))
        {
            return module;
        }
    }

    return &kernel_module;
}

/**
 * Finds the module containing the function that it is used within, for example:
 *
 *   void foo() { module_self(); }
 *
 * foo will receive the module containing itself.
 * 
 * Note that function inlining will cause unexpected results, so do not use
 * this in functions that might be inlined.
 */
__noinline module_t *module_self(void) {
    return module_caller();
}

/** Find a module in the module list.
 * @param name          Name of module to find.
 * @return              Pointer to module structure, NULL if not found. */
static module_t *module_find(const char *name) {
    list_foreach(&module_list, iter) {
        module_t *module = list_entry(iter, module_t, header);

        if (strcmp(module->name, name) == 0)
            return module;
    }

    return NULL;
}

/** Find and check a module's information. */
static status_t find_module_info(module_t *module) {
    symbol_t sym;
    bool found;

    /* Retrieve the module information. */
    found = elf_symbol_lookup(&module->image, "__module_name", false, false, &sym);
    module->name = (found) ? (void *)sym.addr : NULL;
    found = elf_symbol_lookup(&module->image, "__module_desc", false, false, &sym);
    module->description = (found) ? (void *)sym.addr : NULL;
    found = elf_symbol_lookup(&module->image, "__module_init", false, false, &sym);
    module->init = (found) ? (void *)(*(ptr_t *)sym.addr) : NULL;
    found = elf_symbol_lookup(&module->image, "__module_unload", false, false, &sym);
    module->unload = (found) ? (void *)(*(ptr_t *)sym.addr) : NULL;

    /* Check if it is valid. */
    if (!module->name || !module->description || !module->init) {
        kprintf(LOG_NOTICE, "module: information for module '%s' is invalid\n", module->image.name);
        return STATUS_MALFORMED_IMAGE;
    } else if (strnlen(module->name, MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
        kprintf(LOG_NOTICE, "module: name of module '%s' is too long\n", module->image.name);
        return STATUS_MALFORMED_IMAGE;
    } else if (strnlen(module->description, MODULE_DESC_MAX + 1) == (MODULE_DESC_MAX + 1)) {
        kprintf(LOG_NOTICE, "module: description of module '%s' is too long\n", module->image.name);
        return STATUS_MALFORMED_IMAGE;
    }

    /* Check if a module with this name already exists. */
    if (module_find(module->name))
        return STATUS_ALREADY_EXISTS;

    return STATUS_SUCCESS;
}

/** Finish loading a module.
 * @param module        Module being loaded.
 * @param _name         Where to store name of unmet dependency.
 * @return              Status code describing result of the operation. */
static status_t finish_module(module_t *module, const char **_name) {
    status_t ret;

    /* No dependencies symbol means no dependencies. */
    symbol_t sym;
    if (elf_symbol_lookup(&module->image, "__module_deps", false, false, &sym)) {
        module->deps = (const char **)sym.addr;
    } else {
        module->deps = NULL;
    }

    /* Loop through each dependency. The array is NULL-terminated. */
    for (size_t i = 0; module->deps && module->deps[i]; i++) {
        if (strnlen(module->deps[i], MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
            kprintf(LOG_WARN, "module: module %s has invalid dependency\n", module->name);
            return STATUS_MALFORMED_IMAGE;
        }

        module_t *dep = module_find(module->deps[i]);
        if (!dep || module_state(dep) != MODULE_STATE_READY) {
            if (_name)
                *_name = module->deps[i];

            return STATUS_MISSING_LIBRARY;
        }
    }

    /* Perform remaining relocations on the module. At this point all
     * dependencies are loaded, so assuming the module's dependencies are
     * correct, external symbol lookups can be done. */
    ret = elf_module_finish(&module->image);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Call the module initialization function. */
    dprintf(
        "module: calling init function %p for module %p (%s)\n",
        module->init, module, module->name);

    ret = module->init();
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Reference all the dependencies. We leave this until now to avoid having
     * to go through and remove the reference if anything above fails. */
    for (size_t i = 0; module->deps && module->deps[i]; i++) {
        module_t *dep = module_find(module->deps[i]);

        /* This should succeeds since we hold the module lock and we've already
         * checked the state. */
        bool retained __unused = module_retain(dep);
        assert(retained);
    }

    /* Should have no references yet. */
    assert(module->state == MODULE_STATE_LOADING << MODULE_STATE_SHIFT);
    module->state = MODULE_STATE_READY << MODULE_STATE_SHIFT;

    kprintf(
        LOG_NOTICE, "module: successfully loaded module %s (%s)\n",
        module->name, module->description);

    return STATUS_SUCCESS;
}

/**
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided, which should be MODULE_NAME_MAX bytes long.
 * The intended usage of this function is to keep on calling it and loading
 * each unmet dependency it specifies until it succeeds.
 *
 * @param path          Path to module on filesystem.
 * @param depbuf        Where to store name of unmet dependency (should be
 *                      MODULE_NAME_MAX + 1 bytes long).
 *
 * @return              Status code describing result of the operation. If a
 *                      required dependency is not loaded, the function will
 *                      return STATUS_MISSING_LIBRARY.
 */
status_t module_load(const char *path, char *depbuf) {
    status_t ret;

    assert(path);

    /* Open a handle to the file. */
    object_handle_t *handle;
    ret = fs_open(path, FILE_ACCESS_READ, 0, 0, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    module_t *module = kmalloc(sizeof(module_t), MM_KERNEL);

    list_init(&module->header);
    module->state = MODULE_STATE_LOADING << MODULE_STATE_SHIFT;

    /* Take the module lock to serialise module loading. */
    mutex_lock(&module_lock);

    /* Perform first stage of loading the module. */
    ret = elf_module_load(handle, path, &module->image);
    object_handle_release(handle);
    if (ret != STATUS_SUCCESS)
        goto err_unlock;

    ret = find_module_info(module);
    if (ret != STATUS_SUCCESS)
        goto err_destroy;

    list_append(&module_list, &module->header);

    const char *dep;
    ret = finish_module(module, &dep);
    if (ret != STATUS_SUCCESS) {
        if (ret == STATUS_MISSING_LIBRARY && depbuf)
            strncpy(depbuf, dep, MODULE_NAME_MAX + 1);

        list_remove(&module->header);
        goto err_destroy;
    }

    mutex_unlock(&module_lock);
    return STATUS_SUCCESS;

err_destroy:
    elf_module_destroy(&module->image);

err_unlock:
    mutex_unlock(&module_lock);
    kfree(module);
    return ret;
}

/**
 * Unload a kernel module. A module can only be unloaded where there are no
 * other modules loaded which depend on it, and when there are no handles open
 * to any devices it has created.
 *
 * @param name          Name of the module to unload.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_IN_USE if the module is still in use.
 *                      STATUS_NOT_FOUND if the module was not found.
 *                      Any other status code returned by the module's unload
 *                      function.
 */
status_t module_unload(const char *name) {
    /* TODO: Need to atomically test and set the module state to unload ensuring
     * that the count is 0. */
    return STATUS_NOT_IMPLEMENTED;
}

/**
 * Looks for the kernel symbol corresponding to an address in all loaded
 * modules, and gets the offset of the address in the symbol. Note that
 * symbol lookups should only be performed in KDB or by the module loader:
 * they do not take a lock and are therefore unsafe.
 *
 * If a matching symbol is not found, the symbol structure will still be
 * filled in. The symbol name will be set to "<unknown>". If the given address
 * lies within a loaded image, the image pointer will be set to that image,
 * otherwise it will be set to NULL. Everything else will be set to 0.
 *
 * @param addr          Address to lookup.
 * @param symbol        Symbol structure to fill in.
 * @param _off          Where to store symbol offset (can be NULL).
 *
 * @return              Whether a symbol was found for the address.
 */
bool symbol_from_addr(ptr_t addr, symbol_t *symbol, size_t *_off) {
    symbol->image = NULL;

    list_foreach(&module_list, iter) {
        module_t *module = list_entry(iter, module_t, header);

        if (elf_symbol_from_addr(&module->image, addr, symbol, _off)) {
            return true;
        } else if (symbol->image) {
            break;
        }
    }

    symbol->addr = symbol->size = symbol->global = symbol->exported = 0;
    symbol->name = "<unknown>";

    if (_off)
        *_off = 0;

    return false;
}

/**
 * Looks for a symbol with the specified name in all loaded modules. If
 * requested, only global and/or exported symbols will be returned. Note that
 * symbol lookups should only be performed in KDB or by the module loader:
 * they do not take a lock and are therefore unsafe.
 *
 * @param name          Name to lookup.
 * @param global        Whether to only look up global symbols.
 * @param exported      Whether to only look up exported symbols.
 * @param symbol        Symbol structure to fill in.
 *
 * @return              Whether a symbol by this name was found.
 */
bool symbol_lookup(const char *name, bool global, bool exported, symbol_t *symbol) {
    list_foreach(&module_list, iter) {
        module_t *module = list_entry(iter, module_t, header);

        if (elf_symbol_lookup(&module->image, name, global, exported, symbol))
            return true;
    }

    return false;
}

static kdb_status_t kdb_cmd_modules(int argc, char **argv, kdb_filter_t *filter) {
    module_t *module;

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints a list of all loaded kernel modules.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("Name             State     Count Image Description\n");
    kdb_printf("====             =====     ===== ===== ===========\n");

    list_foreach(&module_list, iter) {
        module = list_entry(iter, module_t, header);

        kdb_printf("%-16s ", module->name);

        switch (module_state(module)) {
            case MODULE_STATE_LOADING:
                kdb_printf("Loading   ");
                break;
            case MODULE_STATE_READY:
                kdb_printf("Ready     ");
                break;
            case MODULE_STATE_UNLOADING:
                kdb_printf("Unloading ");
                break;
        }

        kdb_printf(
            "%-5d %-5d %s\n",
            module_count(module), module->image.id, module->description);
    }

    return KDB_SUCCESS;
}

__init_text void module_early_init(void) {
    /* Initialize the kernel module structure. */
    list_init(&kernel_module.header);
    elf_init(&kernel_module.image);

    kernel_module.name        = "kernel";
    kernel_module.description = "Kiwi kernel";
    kernel_module.state       = (MODULE_STATE_READY << MODULE_STATE_SHIFT) | 1;

    list_append(&module_list, &kernel_module.header);

    /* Register the KDB command. */
    kdb_register_command(
        "modules", "Display information about loaded kernel modules.",
        kdb_cmd_modules);
}

static __init_text void finish_boot_module(module_t *module) {
    list_append(&module_list, &module->header);

    while (true) {
        const char *name;
        status_t ret = finish_module(module, &name);
        if (ret == STATUS_MISSING_LIBRARY) {
            /* If it's in the module list we're already trying to load it. */
            if (module_find(name))
                fatal("Circular module dependency detected for %s", name);

            module_t *dep = NULL;
            list_foreach(&boot_module_list, iter) {
                module_t *entry = list_entry(iter, module_t, header);

                if (strcmp(entry->name, name) == 0) {
                    dep = entry;
                    break;
                }
            }

            if (!dep)
                fatal("Boot module %s depends on %s which is not available", module->name, name);

            finish_boot_module(dep);
        } else if (ret != STATUS_SUCCESS) {
            fatal("Failed to load boot module %s (%d)", module->name, ret);
        } else {
            break;
        }
    }
}

__init_text void module_init(void) {
    status_t ret;

    /* Perform the first stage of loading all the modules to find out their
     * names and dependencies. */
    kboot_tag_foreach(KBOOT_TAG_MODULE, kboot_tag_module_t, tag) {
        const char *name        = kboot_tag_data(tag, 0);
        void *mapping           = phys_map(tag->addr, tag->size, MM_BOOT);
        object_handle_t *handle = memory_file_create(mapping, tag->size);

        module_t *module = kmalloc(sizeof(module_t), MM_BOOT);

        list_init(&module->header);
        module->state = MODULE_STATE_LOADING << MODULE_STATE_SHIFT;

        ret = elf_module_load(handle, name, &module->image);
        object_handle_release(handle);
        phys_unmap(mapping, tag->size, true);
        if (ret != STATUS_SUCCESS) {
            if (ret == STATUS_UNKNOWN_IMAGE) {
                /* Assume that it is a filesystem image rather than a module. */
                kfree(module);
                continue;
            }

            fatal("Failed to load boot module %s (%d)", name, ret);
        }

        ret = find_module_info(module);
        if (ret != STATUS_SUCCESS)
            fatal("Boot module %s has invalid information", name);

        list_append(&boot_module_list, &module->header);
    }

    /* Now all of the modules are partially loaded, we can resolve dependencies
     * and load them all in the correct order. */
    while (!list_empty(&boot_module_list)) {
        module_t *module = list_first(&boot_module_list, module_t, header);

        assert(module_state(module) == MODULE_STATE_LOADING);
        finish_boot_module(module);
    }
}

/**
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided. The intended usage of this function is to
 * keep on calling it and loading each unmet dependency it specifies until it
 * succeeds.
 *
 * @param path          Path to module on filesystem.
 * @param depbuf        Where to store name of unmet dependency (should be
 *                      MODULE_NAME_MAX + 1 bytes long).
 *
 * @return              Status code describing result of the operation. If a
 *                      required dependency is not loaded, the function will
 *                      return STATUS_MISSING_LIBRARY.
 */
status_t kern_module_load(const char *path, char *depbuf) {
    status_t ret;

    char *kpath;
    ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
    if (ret != STATUS_SUCCESS)
        return ret;

    char kdepbuf[MODULE_NAME_MAX + 1];
    ret = module_load(kpath, kdepbuf);
    if (ret == STATUS_MISSING_LIBRARY && depbuf) {
        status_t err = memcpy_to_user(depbuf, kdepbuf, MODULE_NAME_MAX + 1);
        if (err != STATUS_SUCCESS)
            ret = err;
    }

    kfree(kpath);
    return ret;
}

#if 0
/** Get information on loaded kernel modules.
 * @param _info         Array of module information structures to fill in. If
 *                      NULL, the function will only return the number of
 *                      loaded modules.
 * @param _count        If _info is not NULL, this should point to a value
 *                      containing the size of the provided array. Upon
 *                      successful completion, the value will be updated to
 *                      be the number of structures filled in. If _info is NULL,
 *                      the number of loaded modules will be stored here.
 * @return              Status code describing result of the operation. */
status_t kern_module_info(module_info_t *_info, size_t *_count) {
    size_t i = 0, count = 0;
    module_info_t info;
    module_t *module;
    status_t ret;

    if (_info) {
        ret = read_user(_count, &count);
        if (ret != STATUS_SUCCESS) {
            return ret;
        } else if (!count) {
            return STATUS_SUCCESS;
        }
    }

    mutex_lock(&module_lock);

    list_foreach(&module_list, iter) {
        if (_info) {
            module = list_entry(iter, module_t, header);

            strcpy(info.name, module->name);
            strcpy(info.desc, module->description);
            info.count = refcount_get(&module->count);
            info.load_size = module->load_size;

            ret = memcpy_to_user(&_info[i], &info, sizeof(info));
            if (ret != STATUS_SUCCESS) {
                mutex_unlock(&module_lock);
                return ret;
            }

            if (++i >= count)
                break;
        } else {
            i++;
        }
    }

    mutex_unlock(&module_lock);
    return write_user(_count, i);
}
#endif
