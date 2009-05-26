/* Kiwi bitmap implementation
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Bitmap data type.
 */

#ifndef __BITMAP_H
#define __BITMAP_H

#include <lib/utility.h>

#include <mm/flags.h>

#include <types.h>

/** Structure containing a bitmap. */
typedef struct bitmap {
	uint8_t *data;		/**< Bitmap data. */
	size_t count;		/**< Number of bits in the bitmap. */
	bool allocated;		/**< Whether data was allocated by bitmap_init(). */
} bitmap_t;

/** Get the number of bytes required for a bitmap. */
#define BITMAP_BYTES(bits)	(ROUND_UP(bits, 8) / 8)

extern int bitmap_init(bitmap_t *bitmap, size_t bits, uint8_t *data, int kmflag);
extern void bitmap_destroy(bitmap_t *bitmap);

extern void bitmap_set(bitmap_t *bitmap, size_t bit);
extern void bitmap_clear(bitmap_t *bitmap, size_t bit);
extern bool bitmap_test(bitmap_t *bitmap, size_t bit);

#endif /* __BITMAP_H */
