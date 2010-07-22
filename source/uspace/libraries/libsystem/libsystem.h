/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Internal libsystem functions.
 */

#ifndef __LIBSYSTEM_H
#define __LIBSYSTEM_H

#include <kernel/process.h>

#define __need_size_t
#define __need_NULL
#include <stddef.h>

extern char **environ;
extern const char *__libsystem_error_list[];
extern size_t __libsystem_error_size;

extern void __libsystem_init(process_args_t *args);
extern void __libsystem_fatal(const char *fmt, ...) __attribute__((noreturn));
extern void __libsystem_stub(const char *name) __attribute__((noreturn));

extern int main(int argc, char **argv, char **envp);

#endif /* __LIBSYSTEM_H */
