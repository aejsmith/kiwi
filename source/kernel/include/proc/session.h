/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Session management.
 */

#ifndef __PROC_SESSION_H
#define __PROC_SESSION_H

#include <lib/refcount.h>

/** Structure containing details of a session. */
typedef struct session {
	session_id_t id;		/**< ID of the session. */
	refcount_t count;		/**< Number of processes in the session. */
} session_t;

extern session_t *session_create(void);
extern void session_get(session_t *session);
extern void session_release(session_t *session);

extern void __init_text session_init(void);

#endif /* __PROC_SESSION_H */
