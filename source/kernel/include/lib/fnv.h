/*
 * Copyright (C) 2007-2011 Alex Smith
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
 * @brief               FNV hash functions.
 *
 * Reference:
 *  - Fowler/Noll/Vo (FNV) Hash
 *    http://www.isthe.com/chongo/tech/comp/fnv/
 */

#ifndef __LIB_FNV_H
#define __LIB_FNV_H

#include <types.h>

/** 32-bit FNV_prime. */
#define FNV_PRIME           16777619ul

/** Result of hashing a known string with the FNV-0 algorithm and the above prime. */
#define FNV_OFFSET_BASIS    2166136261ul

/** Compute the FNV-1 hash of an integer.
 * @param val           Value to hash.
 * @return              Generated hash. */
#define fnv_hash_integer(val)   \
    __extension__ \
    ({ \
        typeof(val) __fnv_val = val; \
        register uint32_t __fnv_hash = FNV_OFFSET_BASIS; \
        size_t __fnv_i = 0; \
        while (__fnv_i++ < sizeof(__fnv_val)) { \
            __fnv_hash = (__fnv_hash * FNV_PRIME) ^ (__fnv_val & 0xff); \
            __fnv_val >>= 8; \
        } \
        __fnv_hash; \
    })

#endif /* __LIB_FNV_H */
