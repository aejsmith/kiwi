/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Assertion function.
 */

#undef assert

#ifdef NDEBUG
#   define assert(cond)   ((void)0)
#else
#   define assert(cond)   \
        if (__builtin_expect(!!(!(cond)), 0)) { \
            __assert_fail(#cond, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
        }
#endif

#ifndef __ASSERT_H
#define __ASSERT_H

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

extern void __assert_fail(const char *cond, const char *file, unsigned int line, const char *func) __sys_noreturn;

__SYS_EXTERN_C_END

#endif /* __ASSERT_H */
