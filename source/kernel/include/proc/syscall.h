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
 * @brief		System call dispatcher.
 */

#ifndef __PROC_SYSCALL_H
#define __PROC_SYSCALL_H

#include <types.h>

struct syscall_frame;

/** Function type for a system call handler. */
typedef unative_t (*syscall_handler_t)(unative_t, unative_t, unative_t, unative_t, unative_t, unative_t);

/** System call service definition structure. */
typedef struct syscall_service {
	syscall_handler_t *table;	/**< Handler table. */
	size_t size;			/**< Size of handler array. */
} syscall_service_t;

extern unative_t syscall_handler(struct syscall_frame *frame);
extern int syscall_service_register(uint16_t num, syscall_service_t *service);

#endif /* __PROC_SYSCALL_H */
