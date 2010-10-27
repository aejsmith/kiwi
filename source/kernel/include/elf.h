/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Kernel ELF loading functions.
 */

#ifndef __KERNEL_ELF_H
#define __KERNEL_ELF_H

#include "../../include/elf.h"
#include <arch/elf.h>

struct object_handle;
struct module;
struct vm_aspace;

extern status_t elf_binary_reserve(struct object_handle *handle, struct vm_aspace *as);
extern status_t elf_binary_load(struct object_handle *handle, struct vm_aspace *as, ptr_t dest, void **datap);
extern ptr_t elf_binary_finish(void *data);

extern status_t elf_module_apply_rel(struct module *module, elf_rel_t *rel, elf_shdr_t *target);
extern status_t elf_module_apply_rela(struct module *module, elf_rela_t *rela, elf_shdr_t *target);
extern status_t elf_module_lookup_symbol(struct module *module, size_t num, elf_addr_t *valp);
extern status_t elf_module_load(struct module *module);
extern status_t elf_module_finish(struct module *module);

#endif /* __KERNEL_ELF_H */
