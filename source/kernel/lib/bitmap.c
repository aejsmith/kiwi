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

#include <arch/bitops.h>

#include <lib/bitmap.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/**
 * Initialize a bitmap.
 *
 * Initializes the given bitmap structure. If the data argument is supplied,
 * then it should point to a preallocated memory area that's large enough to
 * store bits for each bit in the bitmap (see BITMAP_BYTES in bitmap.h).
 * Otherwise, memory for the bitmap will be dynamically allocated. If the
 * bitmap's data was not allocated by this function, then bitmap_destroy()
 * will not free it.
 *
 * @param bitmap	Bitmap to initialize.
 * @param bits		Number of bits the bitmap should be able to hold.
 * @param data		Pointer to preallocated memory (if any).
 * @param mmflag	Allocation flags to use if allocating memory.
 *
 * @return		Status code describing result of the operation. Always
 *			succeeds if using preallocated memory).
 */
status_t bitmap_init(bitmap_t *bitmap, int bits, uint8_t *data, int mmflag) {
	assert(bits > 0);

	if(data) {
		bitmap->allocated = false;
		bitmap->data = data;
	} else {
		bitmap->allocated = true;
		bitmap->data = kmalloc(BITMAP_BYTES(bits), mmflag);
		if(bitmap->data == NULL) {
			return STATUS_NO_MEMORY;
		}

		memset(bitmap->data, 0, BITMAP_BYTES(bits));
	}

	bitmap->count = bits;
	return STATUS_SUCCESS;
}

/**
 * Destroy a bitmap.
 *
 * Destroys the given bitmap. If the original call to bitmap_init() was given
 * a preallocated memory space, then this function will not free it: it is up
 * to the caller to free this data if required.
 *
 * @param bitmap	Bitmap to destroy.
 */
void bitmap_destroy(bitmap_t *bitmap) {
	if(bitmap->allocated) {
		kfree(bitmap->data);
	}
}

/** Set a bit in a bitmap.
 * @param bitmap	Bitmap to set in.
 * @param bit		Number of the bit to set. */
void bitmap_set(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	bitmap->data[bit / 8] |= (1 << (bit % 8));
}

/** Clear a bit in a bitmap.
 * @param bitmap	Bitmap to clear in.
 * @param bit		Number of the bit to clear. */
void bitmap_clear(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	bitmap->data[bit / 8] &= ~(1 << (bit % 8));
}

/** Test whether a bit is set in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @param bit		Number of the bit to test.
 * @return		True if bit set, false if not. */
bool bitmap_test(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	return bitmap->data[bit / 8] & (1 << (bit % 8));
}

/** Find first set bit in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @return		Position of first set bit, -1 if none set. */
int bitmap_ffs(bitmap_t *bitmap) {
	size_t total = bitmap->count;
	unsigned long value;
	int result = 0;

	while(total >= BITS(unsigned long)) {
		value = ((unsigned long *)bitmap->data)[result / BITS(unsigned long)];
		if(value) {
			return result + bitops_ffs(value);
		}

		total -= BITS(unsigned long);
		result += BITS(unsigned long);
	}

	/* Probably could be done faster... */
	while(total) {
		if(bitmap_test(bitmap, result)) {
			return result;
		}

		total--;
		result++;
	}

	return -1;
}

/** Find first zero bit in a bitmap.
 * @param bitmap	Bitmap to test in.
 * @return		Position of first zero bit, -1 if all set. */
int bitmap_ffz(bitmap_t *bitmap) {
	size_t total = bitmap->count;
	unsigned long value;
	int result = 0;

	while(total >= BITS(unsigned long)) {
		value = ((unsigned long *)bitmap->data)[result / BITS(unsigned long)];
		if(value != ~((unsigned long)0)) {
			return result + bitops_ffz(value);
		}

		total -= BITS(unsigned long);
		result += BITS(unsigned long);
	}

	/* Probably could be done faster... */
	while(total) {
		if(!bitmap_test(bitmap, result)) {
			return result;
		}

		total--;
		result++;
	}

	return -1;
}
