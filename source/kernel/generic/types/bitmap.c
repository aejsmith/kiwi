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

#include <arch/bitops.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <types/bitmap.h>

#include <assert.h>
#include <errors.h>

/** Initialize a bitmap.
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
 * @param kmflag	Allocation flags to use if allocating memory.
 *
 * @return		0 on success, negative error code on failure (always
 *			succeeds if using preallocated memory).
 */
int bitmap_init(bitmap_t *bitmap, int bits, uint8_t *data, int kmflag) {
	if(data) {
		bitmap->allocated = false;
		bitmap->data = data;
	} else {
		bitmap->allocated = true;
		bitmap->data = kmalloc(BITMAP_BYTES(bits), kmflag);
		if(bitmap->data == NULL) {
			return -ERR_NO_MEMORY;
		}

		memset(bitmap->data, 0, BITMAP_BYTES(bits));
	}

	bitmap->count = bits;
	return 0;
}

/** Destroy a bitmap.
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
 *
 * Sets the specified bit in a bitmap.
 *
 * @param bitmap	Bitmap to set in.
 * @param bit		Number of the bit to set.
 */
void bitmap_set(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	bitmap->data[bit / 8] |= (1 << (bit % 8));
}

/** Clear a bit in a bitmap.
 *
 * Clears the specified bit in a bitmap.
 *
 * @param bitmap	Bitmap to clear in.
 * @param bit		Number of the bit to clear.
 */
void bitmap_clear(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	bitmap->data[bit / 8] &= ~(1 << (bit % 8));
}

/** Test a bit in a bitmap.
 *
 * Tests whether the specified bit in a bitmap is set.
 *
 * @param bitmap	Bitmap to test in.
 * @param bit		Number of the bit to test.
 *
 * @return		True if bit set, false if not.
 */
bool bitmap_test(bitmap_t *bitmap, int bit) {
	assert(bit < bitmap->count);
	return bitmap->data[bit / 8] & (1 << (bit % 8));
}

/** Find first set bit in a bitmap.
 *
 * Finds the first bit set in a bitmap.
 *
 * @param bitmap	Bitmap to test in.
 *
 * @return		Position of first set bit, -1 if none set.
 */
int bitmap_ffs(bitmap_t *bitmap) {
	size_t total = bitmap->count;
	unative_t value;
	int result = 0;

	while(total >= BITS(unative_t)) {
		value = ((unative_t *)bitmap->data)[result / BITS(unative_t)];
		if(value) {
			return result + bitops_ffs(value);
		}

		total -= BITS(unative_t);
		result += BITS(unative_t);
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
 *
 * Finds the first zero bit in a bitmap.
 *
 * @param bitmap	Bitmap to test in.
 *
 * @return		Position of first zero bit, -1 if all set.
 */
int bitmap_ffz(bitmap_t *bitmap) {
	size_t total = bitmap->count;
	unative_t value;
	int result = 0;

	while(total >= BITS(unative_t)) {
		value = ((unative_t *)bitmap->data)[result / BITS(unative_t)];
		if(value != ~((unative_t)0)) {
			return result + bitops_ffz(value);
		}

		total -= BITS(unative_t);
		result += BITS(unative_t);
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
