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
 * @brief		Thread management functions.
 */

#ifndef __KERNEL_THREAD_H
#define __KERNEL_THREAD_H

#ifdef KERNEL
# include <public/object.h>
#else
# include <kernel/object.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Thread access rights. */
#define THREAD_QUERY		(1<<8)	/**< Query thread information. */

/** Maximum length of a thread name. */
#define THREAD_NAME_MAX		32

extern status_t SYSCALL(thread_create)(const char *name, void *stack, size_t stacksz,
                                       void (*func)(void *), void *arg,
                                       const object_security_t *security,
                                       object_rights_t rights, handle_t *handlep);
extern status_t SYSCALL(thread_open)(thread_id_t id, object_rights_t rights, handle_t *handlep);
extern thread_id_t SYSCALL(thread_id)(handle_t handle);
extern void SYSCALL(thread_exit)(int status) __attribute__((noreturn));
extern status_t SYSCALL(thread_usleep)(useconds_t us, useconds_t *remp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_THREAD_H */
