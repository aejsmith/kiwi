/*
 * Copyright (C) 2009-2020 Alex Smith
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

#ifndef __LIBSYSTEM_H
#define __LIBSYSTEM_H

#include <kernel/types.h>

#include <system/defs.h>

struct process_args;

extern char **environ;
extern const char *__errno_list[];
extern size_t __errno_count;

extern const char *__program_name;

extern void libsystem_init(struct process_args *args);
extern void libsystem_fatal(const char *fmt, ...) __sys_noreturn __sys_hidden;
extern void libsystem_stub(const char *name, bool fatal) __sys_hidden;

extern void libsystem_status_to_errno(status_t status) __sys_hidden;

extern int main(int argc, char **argv, char **envp);

#endif /* __LIBSYSTEM_H */
