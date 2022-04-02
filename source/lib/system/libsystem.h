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

extern void libsystem_main(void);
extern void libsystem_fatal(const char *fmt, ...) __sys_noreturn __sys_hidden;
extern void libsystem_stub(const char *name, bool fatal) __sys_hidden;

#define libsystem_assert(cond) \
    if (__builtin_expect(!!(!(cond)), 0)) { \
        libsystem_assert_fail(#cond, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    }

#define libsystem_log(level, fmt...) \
    core_log(level, "libsystem: " fmt)

extern void libsystem_assert_fail(const char *cond, const char *file, unsigned int line, const char *func) __sys_noreturn __sys_hidden;

extern int libsystem_status_to_errno_val(status_t status) __sys_hidden;
extern void libsystem_status_to_errno(status_t status) __sys_hidden;

extern int main(int argc, char **argv, char **envp);
