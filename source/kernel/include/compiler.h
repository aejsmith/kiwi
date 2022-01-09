/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Compiler-specific macros/definitions.
 */

#pragma once

#include <arch/cache.h>

#ifndef __GNUC__
#   error "Must be compiled with a GCC-compatible compiler"
#endif

#define __unused                __attribute__((unused))
#define __used                  __attribute__((used))
#define __packed                __attribute__((packed))
#define __aligned(a)            __attribute__((aligned(a)))
#define __noreturn              __attribute__((noreturn))
#define __malloc                __attribute__((malloc))
#define __printf(a, b)          __attribute__((format(printf, a, b)))
#define __deprecated            __attribute__((deprecated))
#define __always_inline         __attribute__((always_inline))
#define __noinline              __attribute__((noinline))
#define __cacheline_aligned     __aligned(CPU_CACHE_SIZE)

/**
 * Convenience macro for __attribute__((cleanup)). This will run the specified
 * function when the variable it applies to goes out of scope, with a pointer
 * to the variable as its argument.
 *
 * This can be used to simplify writing cleanup destruction code. This behaves
 * in the same way you would expect C++ to execute destructors for local
 * variables:
 *
 *   {
 *       if (...)
 *           return A;
 * 
 *       void *test __cleanup_kfree = kmalloc(32, MM_KERNEL);
 *
 *       if (...)
 *           return B;
 *   }
 *
 *   <do thing>
 *
 * The "return A" would not execute kfree(), but "return B" would. If no return
 * is hit, kfree() would be called at the closing brace, before "<do thing>".
 *
 * Mixing cleanup variables and gotos should be done with care. Clang will
 * prevent you from doing a goto over the declaration of a cleanup variable
 * into a scope where the variable is still defined. For example, the following
 * is illegal and will result in a compile error:
 *
 *       if (...)
 *           goto fail;
 *
 *       void *test __cleanup_kfree = kmalloc(32, MM_KERNEL);
 *
 *       ...
 *
 *   fail:
 *       ...
 *       return ret;
 *
 * However, wrapping the top part (before the label) inside a new scope would
 * be allowed, since that means test is not in scope inside the label.
 */
#define __cleanup(f)            __attribute__((cleanup(f)))

#ifdef __clang_analyzer__
#   define __init_text
#   define __init_data
#   define __section(s)
#   define __export
#else
#   define __init_text          __attribute__((section(".init.text")))
#   define __init_data          __attribute__((section(".init.data")))
#   define __section(s)         __attribute__((section(s)))
#   define __hidden             __attribute__((visibility("hidden")))
#   define __export             __attribute__((visibility("default")))
#endif

#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)
#define compiler_barrier()      __asm__ volatile("" ::: "memory")
#define unreachable()           __builtin_unreachable()

#define STRINGIFY(val)          #val
#define XSTRINGIFY(val)         STRINGIFY(val)
