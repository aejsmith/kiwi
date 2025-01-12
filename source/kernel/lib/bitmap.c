/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Bitmap data type.
 */

#include <lib/bitmap.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

static unsigned long bit_index(unsigned long bit) {
    return bit / type_bits(unsigned long);
}

static unsigned long bit_offset(unsigned long bit) {
    return bit % type_bits(unsigned long);
}

/**
 * Allocates enough space to store a bitmap of the specified size. The memory
 * is allocated with kmalloc(), and therefore must be freed with kfree().
 *
 * @param nbits         Required size of the bitmap.
 * @param mmflag        Allocation behaviour flags (see mm/mm.h).
 *
 * @return              Pointer to allocated bitmap, or null on failure.
 */
unsigned long *bitmap_alloc(size_t nbits, uint32_t mmflag) {
    unsigned long *bitmap = kmalloc(bitmap_bytes(nbits), mmflag);

    if (likely(bitmap))
        bitmap_zero(bitmap, nbits);

    return bitmap;
}

/** Zero a bitmap.
 * @param bitmap        Bitmap to zero.
 * @param nbits         Size of the bitmap. */
void bitmap_zero(unsigned long *bitmap, size_t nbits) {
    memset(bitmap, 0, bitmap_bytes(nbits));
}

/** Atomically set a bit in a bitmap.
 * @param bitmap        Bitmap to set in.
 * @param bit           Index of the bit to set. */
void bitmap_set(unsigned long *bitmap, unsigned long bit) {
    atomic_fetch_or((atomic_ulong *)&bitmap[bit_index(bit)], (1ul << bit_offset(bit)));
}

/** Atomically clear a bit in a bitmap.
 * @param bitmap        Bitmap to clear in.
 * @param bit           Index of the bit to clear. */
void bitmap_clear(unsigned long *bitmap, unsigned long bit) {
    atomic_fetch_and((atomic_ulong *)&bitmap[bit_index(bit)], ~(1ul << bit_offset(bit)));
}

/** Test whether a bit is set in a bitmap.
 * @param bitmap        Bitmap to test in.
 * @param bit           Index of the bit to test.
 * @return              Whether the bit is set. */
bool bitmap_test(const unsigned long *bitmap, unsigned long bit) {
    return bitmap[bit_index(bit)] & (1ul << bit_offset(bit));
}

/** Find first set bit in a bitmap.
 * @param bitmap        Bitmap to test in.
 * @param nbits         Size of the bitmap.
 * @return              Position of first set bit, -1 if none set. */
long bitmap_ffs(const unsigned long *bitmap, size_t nbits) {
    unsigned long value;
    long result = 0;

    while (nbits >= type_bits(unsigned long)) {
        value = *(bitmap++);
        if (value)
            return result + ffs(value) - 1;

        nbits  -= type_bits(unsigned long);
        result += type_bits(unsigned long);
    }

    if (nbits) {
        /* Must be on the last word here. Select only the bits that are within
         * the bitmap. */
        value = (*bitmap) & (~0ul >> (type_bits(unsigned long) - nbits));
        if (value)
            return result + ffs(value) - 1;
    }

    return -1;
}

/** Find first zero bit in a bitmap.
 * @param bitmap        Bitmap to test in.
 * @param nbits         Size of the bitmap.
 * @return              Position of first zero bit, -1 if all set. */
long bitmap_ffz(const unsigned long *bitmap, size_t nbits) {
    unsigned long value;
    long result = 0;

    while (nbits >= type_bits(unsigned long)) {
        value = *(bitmap++);
        if (value != ~0ul)
            return result + ffz(value) - 1;

        nbits  -= type_bits(unsigned long);
        result += type_bits(unsigned long);
    }

    if (nbits) {
        /* Must be on the last word here. Select only the bits that are within
         * the bitmap. */
        value = (*bitmap) | (~0ul << nbits);
        if (value != ~0ul)
            return result + ffz(value) - 1;
    }

    return -1;
}

/** Find first set bit in a bitmap starting from a given position.
 * @param bitmap        Bitmap to test in.
 * @param nbits         Size of the bitmap.
 * @param from          First bit to check.
 * @return              Position of first set bit, -1 if none set. */
long bitmap_ffs_from(const unsigned long *bitmap, size_t nbits, unsigned long from) {
    if (from == 0)
        return bitmap_ffs(bitmap, nbits);

    unsigned long value;
    long result = 0;

    while (nbits >= type_bits(unsigned long)) {
        value = *(bitmap++);
        if (from < type_bits(unsigned long)) {
            value &= ~0ul << from;
            if (value)
                return result + ffs(value) - 1;

            from = 0;
        } else {
            from -= type_bits(unsigned long);
        }

        nbits  -= type_bits(unsigned long);
        result += type_bits(unsigned long);
    }

    if (nbits && from < type_bits(unsigned long)) {
        /* Select only the bits that are within the bitmap. */
        value = (*bitmap) & (~0ul >> (type_bits(unsigned long) - nbits));
        value &= ~0ul << from;
        if (value)
            return result + ffs(value) - 1;
    }

    return -1;
}

/** Find first zero bit in a bitmap starting from a given position.
 * @param bitmap        Bitmap to test in.
 * @param nbits         Size of the bitmap.
 * @param from          First bit to check.
 * @return              Position of first zero bit, -1 if none zero. */
long bitmap_ffz_from(const unsigned long *bitmap, size_t nbits, unsigned long from) {
    if (from == 0)
        return bitmap_ffz(bitmap, nbits);

    unsigned long value;
    long result = 0;

    while (nbits >= type_bits(unsigned long)) {
        value = *(bitmap++);
        if (from < type_bits(unsigned long)) {
            value |= (1 << from) - 1;
            if (value != ~0ul)
                return result + ffz(value) - 1;

            from = 0;
        } else {
            from -= type_bits(unsigned long);
        }

        nbits  -= type_bits(unsigned long);
        result += type_bits(unsigned long);
    }

    if (nbits && from < type_bits(unsigned long)) {
        /* Select only the bits that are within the bitmap. */
        value = (*bitmap) | (~0ul << nbits);
        value |= (1 << from) - 1;
        if (value != ~0ul)
            return result + ffz(value) - 1;
    }

    return -1;
}
