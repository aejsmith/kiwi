/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		ID allocator.
 */

#ifndef __LIB_ID_ALLOC_H
#define __LIB_ID_ALLOC_H

#include <lib/bitmap.h>
#include <sync/mutex.h>

/** ID allocator structure. */
typedef struct id_alloc {
	mutex_t lock;			/**< Lock to protect the allocator. */
	bitmap_t bitmap;		/**< Bitmap of IDs. */
} id_alloc_t;

extern int32_t id_alloc_get(id_alloc_t *alloc);
extern void id_alloc_release(id_alloc_t *alloc, int32_t id);
extern void id_alloc_reserve(id_alloc_t *alloc, int32_t id);
extern void id_alloc_init(id_alloc_t *alloc, int32_t max);

#endif /* __LIB_ID_ALLOC_H */
