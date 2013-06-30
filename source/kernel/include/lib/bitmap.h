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
 * @brief		Bitmap implementation.
 */

#ifndef __LIB_BITMAP_H
#define __LIB_BITMAP_H

#include <lib/utility.h>

#include <mm/mm.h>

/** Get the number of bytes required for a bitmap. */
#define BITMAP_BYTES(nbits)	(ROUND_UP(nbits, 8) / 8)

extern unsigned long *bitmap_alloc(size_t nbits, int mmflag);
extern void bitmap_zero(unsigned long *bitmap, size_t nbits);
extern void bitmap_set(unsigned long *bitmap, unsigned long bit);
extern void bitmap_clear(unsigned long *bitmap, unsigned long bit);
extern bool bitmap_test(const unsigned long *bitmap, unsigned long bit);
extern long bitmap_ffs(const unsigned long *bitmap, size_t nbits);
extern long bitmap_ffz(const unsigned long *bitmap, size_t nbits);
extern long bitmap_next(const unsigned long *bitmap, size_t nbits, unsigned long current);

#endif /* __LIB_BITMAP_H */
