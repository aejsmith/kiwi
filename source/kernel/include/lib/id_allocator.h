/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Object ID allocator.
 */

#ifndef __LIB_ID_ALLOCATOR_H
#define __LIB_ID_ALLOCATOR_H

#include <sync/mutex.h>

/** ID allocator structure. */
typedef struct id_allocator {
	mutex_t lock;			/**< Lock to protect the allocator. */
	unsigned long *bitmap;		/**< Bitmap of IDs. */
	size_t nbits;			/**< Number of bits in the bitmap. */
} id_allocator_t;

extern int32_t id_allocator_alloc(id_allocator_t *alloc);
extern void id_allocator_free(id_allocator_t *alloc, int32_t id);
extern void id_allocator_reserve(id_allocator_t *alloc, int32_t id);

extern status_t id_allocator_init(id_allocator_t *alloc, int32_t max, int mmflag);
extern void id_allocator_destroy(id_allocator_t *alloc);

#endif /* __LIB_ID_ALLOCATOR_H */
