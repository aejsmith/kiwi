/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Internal image loader functions.
 */

#ifndef __KERNEL_PRIVATE_IMAGE_H
#define __KERNEL_PRIVATE_IMAGE_H

#include <kernel/image.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure containing image information for the kernel. */
typedef struct image_info {
	const char *name;		/**< Name of the image. */
	void *ehdr;			/**< ELF executable header. */
	void *shdrs;			/**< ELF section headers. */
	void *load_base;		/**< Address of allocation module is loaded to. */
	size_t load_size;		/**< Size of allocation module is loaded to. */
} image_info_t;

extern status_t kern_image_register(image_info_t *info, image_id_t **idp);
extern status_t kern_image_unregister(image_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PRIVATE_IMAGE_H */
