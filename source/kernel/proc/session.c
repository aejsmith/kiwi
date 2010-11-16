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

#include <mm/malloc.h>

#include <proc/session.h>

#include <console.h>
#include <vmem.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Session ID allocator. */
static vmem_t *session_id_arena;

/** Create a new session.
 * @return		Pointer to created session, has 1 reference on. */
session_t *session_create(void) {
	session_t *session;

	session = kmalloc(sizeof(*session), MM_SLEEP);
	refcount_set(&session->count, 1);
	session->id = (session_id_t)vmem_alloc(session_id_arena, 1, MM_SLEEP);
	dprintf("session: created session %d\n", session->id);
	return session;
}

/** Increase a session's reference count.
 * @param session	Session to reference. */
void session_get(session_t *session) {
	refcount_inc(&session->count);
}

/** Decrease a session's reference count.
 * @param session	Session to release. */
void session_release(session_t *session) {
	if(refcount_dec(&session->count) == 0) {
		dprintf("session: destroyed session %d\n", session->id);
		vmem_free(session_id_arena, session->id, 1);
		kfree(session);
	}
}

/** Initialise the session ID allocator. */
void __init_text session_init(void) {
	session_id_arena = vmem_create("session_id_arena", 0, 65536, 1, NULL, NULL, NULL, 0, 0, 0, MM_FATAL);
}
