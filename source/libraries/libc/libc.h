/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Internal C library functions.
 */

#ifndef __LIBC_H
#define __LIBC_H

#include <kernel/types.h>

/** Compiler attribute/builtin macros. */
#define __hidden		__attribute__((visibility("hidden")))
#define __noreturn		__attribute__((noreturn))
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

/** Get the number of elements in an array. */
#define ARRAYSZ(a)		(sizeof((a)) / sizeof((a)[0]))

struct process_args;

extern char **environ;
extern const char *__libc_error_list[];
extern size_t __libc_error_size;

extern void libc_init(struct process_args *args);
extern void libc_fatal(const char *fmt, ...) __noreturn __hidden;
extern void libc_stub(const char *name, bool fatal) __hidden;

extern void libc_status_to_errno(status_t status) __hidden;

extern int main(int argc, char **argv, char **envp);

#endif /* __LIBC_H */
