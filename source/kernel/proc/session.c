/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Session management.
 */

#include <lib/id_allocator.h>
#include <mm/malloc.h>
#include <proc/session.h>
#include <kernel.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Session ID allocator. */
static id_allocator_t session_id_allocator;

/** Create a new session.
 * @return		Pointer to created session with 1 reference on, or NULL
 *			if the session limit has been reached. */
session_t *session_create(void) {
	session_t *session;

	session = kmalloc(sizeof(*session), MM_WAIT);
	refcount_set(&session->count, 1);
	session->id = id_allocator_alloc(&session_id_allocator);
	if(session->id < 0) {
		kfree(session);
		return NULL;
	}

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
		id_allocator_free(&session_id_allocator, session->id);
		kfree(session);
	}
}

/** Initialize the session ID allocator. */
__init_text void session_init(void) {
	id_allocator_init(&session_id_allocator, 4095, MM_BOOT);
}
