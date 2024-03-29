/*
 * Copyright (C) 2009-2023 Alex Smith
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

#pragma once

#include <types.h>

/** 32-bit FNV_prime. */
#define FNV32_PRIME         16777619ul

/** Result of hashing a known string with the FNV-0 algorithm and the above prime. */
#define FNV32_OFFSET_BASIS  2166136261ul

/** Compute the FNV-1 hash of an integer.
 * @param val           Value to hash.
 * @return              Generated hash. */
#define fnv32_hash_integer(val)   \
    __extension__ \
    ({ \
        typeof(val) __fnv_val = val; \
        uint32_t __fnv_hash = FNV32_OFFSET_BASIS; \
        for (size_t __fnv_i = 0; __fnv_i < sizeof(__fnv_val); __fnv_i++) { \
            __fnv_hash = (__fnv_hash * FNV32_PRIME) ^ (__fnv_val & 0xff); \
            __fnv_val >>= 8; \
        } \
        __fnv_hash; \
    })

/** Compute the FNV-1 hash of a string.
 * @param val           Value to hash.
 * @return              Generated hash. */
static inline uint32_t fnv32_hash_string(const char *val) {
    uint32_t hash = FNV32_OFFSET_BASIS;

    for (size_t i = 0; val[i]; i++)
        hash = (hash * FNV32_PRIME) ^ (uint8_t)val[i];

    return hash;
}
