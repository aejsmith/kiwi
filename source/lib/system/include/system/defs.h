/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
#define __sys_init_prio(p)  __attribute__((constructor(p)))
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
