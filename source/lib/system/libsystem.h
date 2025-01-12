/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Internal libsystem definitions.
 */

#pragma once

#include <core/log.h>

#include <kernel/types.h>

#include <system/defs.h>

extern char **environ;
extern const char *__errno_list[];
extern size_t __errno_count;

extern const char *__program_name;

/** Constructor function priorities. */
#define LIBSYSTEM_INIT_PRIO_ARGS                0
#define LIBSYSTEM_INIT_PRIO_STDIO               1
#define LIBSYSTEM_INIT_PRIO_CORE_SERVICE        2
#define LIBSYSTEM_INIT_PRIO_POSIX_SERVICE       3
#define LIBSYSTEM_INIT_PRIO_POSIX_SIGNAL        4
#define LIBSYSTEM_INIT_PRIO_PTHREAD             5
#define LIBSYSTEM_INIT_PRIO_PTHREAD_SPECIFIC    6
#define LIBSYSTEM_INIT_PRIO_POSIX_UMASK         7

extern void libsystem_main(void);
extern void libsystem_fatal(const char *fmt, ...) __sys_noreturn __sys_hidden;
extern void libsystem_stub(const char *name, bool fatal) __sys_hidden;

#define libsystem_assert(cond) \
    if (__builtin_expect(!!(!(cond)), 0)) { \
        libsystem_assert_fail(#cond, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    }

#define libsystem_log(level, fmt...) \
    core_log(level, "libsystem: " fmt)

extern void libsystem_assert_fail(
    const char *cond, const char *file, unsigned int line, const char *func) __sys_noreturn __sys_hidden;

extern int libsystem_status_to_errno_val(status_t status) __sys_hidden;
extern void libsystem_status_to_errno(status_t status) __sys_hidden;

extern int main(int argc, char **argv, char **envp);
