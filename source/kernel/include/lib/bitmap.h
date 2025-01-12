/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Bitmap implementation.
 */

#pragma once

#include <lib/utility.h>

#include <mm/mm.h>

/** Get the number of bytes required for a bitmap.
 * @param nbits         Number of bits.
 * @return              Number of bytes required to store the bitmap. */
static inline size_t bitmap_bytes(size_t nbits) {
    return round_up(nbits, 8) / 8;
}

extern unsigned long *bitmap_alloc(size_t nbits, uint32_t mmflag);
extern void bitmap_zero(unsigned long *bitmap, size_t nbits);
extern void bitmap_set(unsigned long *bitmap, unsigned long bit);
extern void bitmap_clear(unsigned long *bitmap, unsigned long bit);
extern bool bitmap_test(const unsigned long *bitmap, unsigned long bit);
extern long bitmap_ffs(const unsigned long *bitmap, size_t nbits);
extern long bitmap_ffz(const unsigned long *bitmap, size_t nbits);
extern long bitmap_ffs_from(const unsigned long *bitmap, size_t nbits, unsigned long from);
extern long bitmap_ffz_from(const unsigned long *bitmap, size_t nbits, unsigned long from);
