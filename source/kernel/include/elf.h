/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ELF loader.
 */

#pragma once

#include "../../lib/system/include/elf.h"

#include <arch/elf.h>

#include <object.h>

struct process;
struct symbol;
struct vm_aspace;

/** ELF image information structure. */
typedef struct elf_image {
    list_t header;                  /**< List to loaded image list. */
    /* Note that if this is ever reordered, gdb_extensions must be updated. */

    image_id_t id;                  /**< ID of the image. */
    char *name;                     /**< Name of the image. */
    char *path;                     /**< Path the image was loaded from (user only). */
    ptr_t load_base;                /**< Base address of image.. */
    size_t load_size;               /**< Total size of image. */
    elf_ehdr_t *ehdr;               /**< ELF executable header (modules only). */
    elf_phdr_t *phdrs;              /**< Program headers (only valid during loading). */
    elf_shdr_t *shdrs;              /**< ELF section headers (modules only). */

    /** Symbol/string tables.
     * @warning             For user images, these are user pointers. */
    void *symtab;                   /**< Symbol table. */
    uint32_t sym_size;              /**< Size of symbol table. */
    uint32_t sym_entsize;           /**< Size of a single symbol table entry. */
    void *strtab;                   /**< String table. */
} elf_image_t;

extern status_t elf_binary_reserve(object_handle_t *handle, struct vm_aspace *as);
extern status_t elf_binary_load(
    object_handle_t *handle, const char *path, struct vm_aspace *as,
    elf_image_t **_image);
extern ptr_t elf_binary_finish(elf_image_t *image);
extern void elf_binary_destroy(elf_image_t *image);

extern status_t arch_elf_module_relocate_rel(elf_image_t *image, elf_rel_t *rel, elf_shdr_t *target);
extern status_t arch_elf_module_relocate_rela(elf_image_t *image, elf_rela_t *rela, elf_shdr_t *target);

extern status_t elf_module_resolve(elf_image_t *image, size_t num, elf_addr_t *_val);

extern status_t elf_module_load(object_handle_t *handle, const char *path, elf_image_t *image);
extern status_t elf_module_finish(elf_image_t *image);
extern void elf_module_destroy(elf_image_t *image);

extern bool elf_symbol_from_addr(elf_image_t *image, ptr_t addr, struct symbol *symbol, size_t *_off);
extern bool elf_symbol_lookup(
    elf_image_t *image, const char *name, bool global, bool exported,
    struct symbol *symbol);

extern void elf_init(elf_image_t *image);

extern void elf_process_clone(struct process *process, struct process *parent);
extern void elf_process_cleanup(struct process *process);
