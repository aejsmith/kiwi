/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Compiler-specific macros/definitions.
 */

#ifndef __COMPILER_H
#define __COMPILER_H

#include <arch/cache.h>

#ifdef __GNUC__
# define __unused		__attribute__((unused))
# define __used			__attribute__((used))
# define __packed		__attribute__((packed))
# define __aligned(a)		__attribute__((aligned(a)))
# define __noreturn		__attribute__((noreturn))
# define __malloc		__attribute__((malloc))
# define __printf(a, b)		__attribute__((format(printf, a, b)))
# define __deprecated		__attribute__((deprecated))
# define __init_text		__attribute__((section(".init.text")))
# define __init_data		__attribute__((section(".init.data")))
# define __section(s)		__attribute__((section(s)))
# define __cacheline_aligned	__aligned(CPU_CACHE_SIZE)
# define likely(x)		__builtin_expect(!!(x), 1)
# define unlikely(x)		__builtin_expect(!!(x), 0)
#else
# error "Kiwi does not currently support compilers other than GCC"
#endif

#endif /* __COMPILER_H */
