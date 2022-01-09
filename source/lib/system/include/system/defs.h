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
 * @brief               Internal libsystem definitions.
 */

#pragma once

#include <system/arch/defs.h>

/** Compiler attribute definitions. */
#define __sys_unused        __attribute__((unused))
#define __sys_used          __attribute__((used))
#define __sys_packed        __attribute__((packed))
#define __sys_aligned(a)    __attribute__((aligned(a)))
#define __sys_noreturn      __attribute__((noreturn))
#define __sys_malloc        __attribute__((malloc))
#define __sys_printf(a, b)  __attribute__((format(printf, a, b)))
#define __sys_deprecated    __attribute__((deprecated))
#define __sys_init          __attribute__((constructor))
#define __sys_fini          __attribute__((destructor))
#define __sys_export        __attribute__((visibility("default")))
#define __sys_hidden        __attribute__((visibility("hidden")))
#define __sys_cleanup(f)    __attribute__((cleanup(f)))

#ifdef __cplusplus
    #define __SYS_EXTERN_C_BEGIN    extern "C" {
    #define __SYS_EXTERN_C_END      }
#else
    #define __SYS_EXTERN_C_BEGIN
    #define __SYS_EXTERN_C_END
#endif
