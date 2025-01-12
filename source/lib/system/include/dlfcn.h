/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Dynamic linking functions.
 */

#pragma once

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

/** Mode flags for dlopen(). */
#define RTLD_LAZY       (1<<0)      /**< Relocations are performed as needed. */
#define RTLD_NOW        (1<<1)      /**< Relocations are performed immediately. */
#define RTLD_GLOBAL     (1<<2)      /**< Module is available for linking to other modules. */
#define RTLD_LOCAL      (1<<3)      /**< Module is not available for linking to other modules. */

/** Symbol information. */
typedef struct dl_info {
    const char *dli_fname;          /**< Pathname of shared object. */
    void *dli_fbase;                /**< Address at which shared object is loaded. */
    const char *dli_sname;          /**< Name of nearest symbol. */
    void *dli_saddr;                /**< Exact address of named symbol. */
} Dl_info;

extern int dlclose(void *handle);
extern char *dlerror(void);
extern void *dlopen(const char *file, int mode);
extern void *dlsym(void *__restrict handle, const char *__restrict name);
extern int dladdr(void *addr, Dl_info *info);

__SYS_EXTERN_C_END
