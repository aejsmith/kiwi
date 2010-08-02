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
 * @brief		Internal C library functions.
 */

#ifndef __LIBC_H
#define __LIBC_H

/** Compiler attribute/builtin macros. */
#define __hidden		__attribute__((visibility("hidden")))
#define __noreturn		__attribute__((noreturn))
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#define __need_size_t
#define __need_NULL
#include <stddef.h>

struct process_args;

extern char **environ;
extern const char *__libc_error_list[];
extern size_t __libc_error_size;

extern void __libc_init(struct process_args *args);
extern void __libc_fatal(const char *fmt, ...) __noreturn __hidden;
extern void __libc_stub(const char *name) __noreturn __hidden;

extern int main(int argc, char **argv, char **envp);

#endif /* __LIBC_H */
