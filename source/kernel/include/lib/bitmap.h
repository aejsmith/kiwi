/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Bitmap data type.
 */

#ifndef __LIB_BITMAP_H
#define __LIB_BITMAP_H

#include <lib/utility.h>

#include <mm/flags.h>

/** Structure containing a bitmap. */
typedef struct bitmap {
	uint8_t *data;		/**< Bitmap data. */
	int count;		/**< Number of bits in the bitmap. */
	bool allocated;		/**< Whether data was allocated by bitmap_init(). */
} bitmap_t;

/** Get the number of bytes required for a bitmap. */
#define BITMAP_BYTES(bits)	(ROUND_UP(bits, 8) / 8)

extern status_t bitmap_init(bitmap_t *bitmap, int bits, uint8_t *data, int mmflag);
extern void bitmap_destroy(bitmap_t *bitmap);

extern void bitmap_set(bitmap_t *bitmap, int bit);
extern void bitmap_clear(bitmap_t *bitmap, int bit);
extern bool bitmap_test(bitmap_t *bitmap, int bit);
extern int bitmap_ffs(bitmap_t *bitmap);
extern int bitmap_ffz(bitmap_t *bitmap);

#endif /* __LIB_BITMAP_H */
