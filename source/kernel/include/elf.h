/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Kernel ELF loading functions.
 */

#ifndef __KERNEL_ELF_H
#define __KERNEL_ELF_H

#include "../../lib/system/include/elf.h"
#include <arch/elf.h>

struct object_handle;
struct module;
struct vm_aspace;

extern status_t elf_binary_reserve(struct object_handle *handle, struct vm_aspace *as);
extern status_t elf_binary_load(struct object_handle *handle, struct vm_aspace *as,
	ptr_t dest, const char *path, void **datap);
extern ptr_t elf_binary_finish(void *data);

extern status_t elf_module_apply_rel(struct module *module, elf_rel_t *rel,
	elf_shdr_t *target);
extern status_t elf_module_apply_rela(struct module *module, elf_rela_t *rela,
	elf_shdr_t *target);
extern status_t elf_module_lookup_symbol(struct module *module, size_t num,
	elf_addr_t *valp);
extern status_t elf_module_load(struct module *module);
extern status_t elf_module_finish(struct module *module);

#endif /* __KERNEL_ELF_H */
