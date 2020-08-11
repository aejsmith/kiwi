/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               AMD64 bit operations.
 */

#ifndef __ARCH_BITOPS_H
#define __ARCH_BITOPS_H

#include <types.h>

/** Atomically set a bit in a bitmap.
 * @param bitmap        Bitmap to set in.
 * @param bit           Bit to set. */
static inline void set_bit(volatile unsigned long *addr, unsigned long bit) {
    __asm__ volatile("lock bts %1, %0" : "+m"(*addr) : "r"(bit) : "memory");
}

/** Atomically clear a bit in a bitmap.
 * @param bitmap        Bitmap to clear in.
 * @param bit           Bit to clear. */
static inline void clear_bit(volatile unsigned long *addr, unsigned long bit) {
    __asm__ volatile("lock btr %1, %0" : "+m"(*addr) : "r"(bit) : "memory");
}

/** Find first set bit in a native-sized value.
 * @note                Does not check if value is zero, caller should do so.
 * @param value         Value to test.
 * @return              Position of first set bit. */
static inline unsigned long ffs(unsigned long value) {
    __asm__("bsf %1, %0" : "=r"(value) : "rm"(value) : "cc");
    return value;
}

/** Find first zero bit in a native-sized value.
 * @note                Does not check if all bits are set, caller should do so.
 * @param value         Value to test.
 * @return              Position of first zero bit. */
static inline unsigned long ffz(unsigned long value) {
    __asm__("bsf %1, %0" : "=r"(value) : "r"(~value) : "cc");
    return value;
}

/** Find last set bit in a native-sized value.
 * @note                Does not check if value is zero, caller should do so.
 * @param value         Value to test.
 * @return              Oosition of last set bit. */
static inline unsigned long fls(unsigned long value) {
    __asm__("bsr %1, %0" : "=r" (value) : "rm"(value) : "cc");
    return value;
}

#endif /* __ARCH_BITOPS_H */
