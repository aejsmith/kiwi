/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Internal image loader functions.
 */

#pragma once

#include <kernel/image.h>

__KERNEL_EXTERN_C_BEGIN

#ifdef __KERNEL_PRIVATE

/** Structure containing image information for the kernel. */
typedef struct image_info {
    const char *name;               /**< Name of the image. */
    const char *path;               /**< Path that the image was loaded from. */
    void *load_base;                /**< Base address of image for relocatable images. */
    size_t load_size;               /**< Size of image for relocatable images. */
    void *symtab;                   /**< Symbol table. */
    uint32_t sym_size;              /**< Size of symbol table. */
    uint32_t sym_entsize;           /**< Size of a single symbol table entry. */
    void *strtab;                   /**< String table. */
} image_info_t;

extern status_t kern_image_register(image_id_t id, image_info_t *info);
extern status_t kern_image_unregister(image_id_t id);

#endif /* __KERNEL_PRIVATE */

__KERNEL_EXTERN_C_END
