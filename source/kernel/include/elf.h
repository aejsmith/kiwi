/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		ELF loader.
 */

#ifndef __KERNEL_ELF_H
#define __KERNEL_ELF_H

#include "../../lib/system/include/elf.h"

#include <arch/elf.h>

#include <object.h>

struct process;
struct symbol;
struct vm_aspace;

/** ELF image information structure. */
typedef struct elf_image {
	list_t header;			/**< List to loaded image list. */

	image_id_t id;			/**< ID of the image. */
	char *name;			/**< Name of the image. */
	ptr_t load_base;		/**< Base address of image.. */
	size_t load_size;		/**< Total size of image. */
	elf_ehdr_t *ehdr;		/**< ELF executable header. */
	elf_phdr_t *phdrs;		/**< Program headers (only valid during loading). */
	elf_shdr_t *shdrs;		/**< ELF section headers. */

	/** Symbol/string tables.
	 * @warning		For user images, these are user pointers. */
	void *symtab;			/**< Symbol table. */
	uint32_t sym_size;		/**< Size of symbol table. */
	uint32_t sym_entsize;		/**< Size of a single symbol table entry. */
	void *strtab;			/**< String table. */
} elf_image_t;

extern status_t elf_binary_reserve(object_handle_t *handle, struct vm_aspace *as);
extern status_t elf_binary_load(object_handle_t *handle, const char *path,
	struct vm_aspace *as, ptr_t dest, elf_image_t **imagep);
extern ptr_t elf_binary_finish(elf_image_t *image);

extern status_t arch_elf_module_relocate_rel(elf_image_t *image, elf_rel_t *rel,
	elf_shdr_t *target);
extern status_t arch_elf_module_relocate_rela(elf_image_t *image, elf_rela_t *rela,
	elf_shdr_t *target);

extern status_t elf_module_resolve(elf_image_t *image, size_t num, elf_addr_t *valp);

extern status_t elf_module_load(object_handle_t *handle, const char *path,
	elf_image_t *image);
extern status_t elf_module_finish(elf_image_t *image);
extern void elf_module_destroy(elf_image_t *image);

extern bool elf_symbol_from_addr(elf_image_t *image, ptr_t addr, struct symbol *symbol,
	size_t *offp);
extern bool elf_symbol_lookup(elf_image_t *image, const char *name, bool global,
	bool exported, struct symbol *symbol);

extern void elf_init(elf_image_t *image);

extern void elf_cleanup(struct process *process);

#endif /* __KERNEL_ELF_H */
