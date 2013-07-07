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
 * @brief		Bitmap data type.
 */

#include <arch/bitops.h>

#include <lib/bitmap.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/** Get the array index containing a bit. */
#define BIT_INDEX(bit)		(bit / BITS(unsigned long))

/** Get the array entry offset of a bit. */
#define BIT_OFFSET(bit)		(bit % BITS(unsigned long))

/**
 * Allocate a new bitmap.
 *
 * Allocates enough space to store a bitmap of the specified size. The memory
 * is allocated with kmalloc(), and therefore must be freed with kfree().
 *
 * @param nbits		Required size of the bitmap.
 * @param mmflag	Allocation behaviour flags (see mm/mm.h).
 *
 * @return		Pointer to allocated bitmap, or null on failure.
 */
unsigned long *bitmap_alloc(size_t nbits, unsigned mmflag) {
	unsigned long *bitmap = kmalloc(BITMAP_BYTES(nbits), mmflag);

	if(likely(bitmap))
		bitmap_zero(bitmap, nbits);

	return bitmap;
}

/** Zero a bitmap.
 * @param bitmap	Bitmap to zero.
 * @param nbits		Size of the bitmap. */
void bitmap_zero(unsigned long *bitmap, size_t nbits) {
	memset(bitmap, 0, BITMAP_BYTES(nbits));
}

/** Atomically set a bit in a bitmap.
 * @param bitmap	Bitmap to set in.
 * @param bit		Index of the bit to set. */
void bitmap_set(unsigned long *bitmap, unsigned long bit) {
	set_bit(&bitmap[BIT_INDEX(bit)], BIT_OFFSET(bit));
}

/** Atomically clear a bit in a bitmap.
 * @param bitmap	Bitmap to clear in.
 * @param bit		Index of the bit to clear. */
void bitmap_clear(unsigned long *bitmap, unsigned long bit) {
	clear_bit(&bitmap[BIT_INDEX(bit)], BIT_OFFSET(bit));
}

/** Test whether a bit is set in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @param bit		Index of the bit to test.
 * @return		Whether the bit is set. */
bool bitmap_test(const unsigned long *bitmap, unsigned long bit) {
	return bitmap[BIT_INDEX(bit)] & (1UL << BIT_OFFSET(bit));
}

/** Find first set bit in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @param nbits		Size of the bitmap.
 * @return		Position of first set bit, -1 if none set. */
long bitmap_ffs(const unsigned long *bitmap, size_t nbits) {
	unsigned long value;
	long result = 0;

	while(nbits >= BITS(unsigned long)) {
		value = *(bitmap++);
		if(value)
			return result + ffs(value);

		nbits -= BITS(unsigned long);
		result += BITS(unsigned long);
	}

	if(nbits) {
		/* Must be on the last word here. Select only the bits that are
		 * within the bitmap. */
		value = (*bitmap) & (~0UL >> (BITS(unsigned long) - nbits));
		if(value)
			return result + ffs(value);
	}

	return -1;
}

/** Find first zero bit in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @param nbits		Size of the bitmap.
 * @return		Position of first zero bit, -1 if all set. */
long bitmap_ffz(const unsigned long *bitmap, size_t nbits) {
	unsigned long value;
	long result = 0;

	while(nbits >= BITS(unsigned long)) {
		value = *(bitmap++);
		if(value != ~0UL)
			return result + ffz(value);

		nbits -= BITS(unsigned long);
		result += BITS(unsigned long);
	}

	if(nbits) {
		/* Must be on the last word here. Select only the bits that are
		 * within the bitmap. */
		value = (*bitmap) | (~0UL << nbits);
		if(value != ~0UL)
			return result + ffz(value);
	}

	return -1;
}

/** Find next set bit in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @param nbits		Size of the bitmap.
 * @param current	Current bit.
 * @return		Position of next bit set, -1 if no more set bits. */
long bitmap_next(const unsigned long *bitmap, size_t nbits, unsigned long current) {
	unsigned long value;
	long result = 0;

	/* Want to start looking from the one after. */
	current += 1;

	while(nbits >= BITS(unsigned long)) {
		value = *(bitmap++);
		if(current < BITS(unsigned long)) {
			value &= ~0UL << current;
			if(value)
				return result + ffs(value);

			current = 0;
		} else {
			current -= BITS(unsigned long);
		}

		nbits -= BITS(unsigned long);
		result += BITS(unsigned long);
	}

	if(nbits) {
		/* Select only the bits that are within the bitmap. */
		value = (*bitmap) & (~0UL >> (BITS(unsigned long) - nbits));
		value &= ~0UL << current;
		if(value)
			return result + ffs(value);
	}

	return -1;
}
