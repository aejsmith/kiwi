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
 * @brief               Assertion function.
 */

#pragma once

#include <kernel.h>

#if CONFIG_DEBUG

/** Raise a fatal error if the given condition is not met.
 * @param cond          Condition to test. */
#define assert(cond)    \
    if (unlikely(!(cond))) \
        __assert_fail(#cond, __FILE__, __LINE__);

#else

#define assert(cond)    ((void)0)

#endif

extern void __assert_fail(const char *cond, const char *file, int line) __noreturn;

#ifndef __cplusplus
#   define static_assert(cond, err)    _Static_assert(cond, err)
#endif
