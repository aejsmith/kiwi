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
 * @brief               Memory allocation tracing helpers.
 */

#pragma once

#include <lib/string.h>

#include <module.h>

/* Our usage of __builtin_return_address is OK, disable errors. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wframe-address"

/** Get the address for allocation tracing output. */
static inline __always_inline void *mm_trace_return_address(const char **skip, size_t count) {
    void *addr = __builtin_return_address(0);

    if (count > 0) {
        symbol_t sym;
        if (!symbol_from_addr((ptr_t)addr - 1, &sym, NULL))
            return addr;

        /* If we're called through another allocation function, we want the
         * address printed to be the caller of that. This can be multiple levels
         * deep, e.g. kstrdup -> kmalloc -> slab_cache_alloc. We have to pass a
         * constant integer to __builtin_return_address, so hardcode this for 2
         * levels deep. Yeah, this is terribly inefficient, but this is only
         * enabled for debugging that makes things terribly slow anyway. */
        for (size_t i = 0; i < count; i++) {
            if (strcmp(skip[i], sym.name) == 0) {
                addr = __builtin_return_address(1);
                if (!symbol_from_addr((ptr_t)addr - 1, &sym, NULL))
                    return addr;

                for (size_t j = 0; j < count; j++) {
                    if (strcmp(skip[j], sym.name) == 0)
                        return __builtin_return_address(2);
                }

                return addr;
            }
        }
    }

    return addr;
}

#pragma clang diagnostic pop
