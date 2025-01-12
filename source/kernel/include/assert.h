/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
