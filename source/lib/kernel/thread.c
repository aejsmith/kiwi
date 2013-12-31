/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Thread functions.
 */

#include <kernel/futex.h>
#include <kernel/mutex.h>
#include <kernel/object.h>
#include <kernel/private/thread.h>

#include "libkernel.h"

/** Thread creation lock. */
static int32_t thread_create_lock = MUTEX_INITIALIZER;

/** Saved ID for the current thread. */
__thread thread_id_t curr_thread_id = -1;

/** Information used by thread_create(). */
typedef struct thread_create {
	status_t ret;			/**< Initialisation status. */
	const thread_entry_t *entry;	/**< Real entry structure. */
} thread_create_t;

/** Thread entry wrapper.
 * @param arg		Pointer to information structure. */
static void thread_entry_wrapper(void *arg) {
	thread_create_t *create = arg;
	const thread_entry_t *entry = create->entry;

	/* Attempt to initialise our TLS block. */
	create->ret = tls_init();
	kern_mutex_unlock(&thread_create_lock);
	if(create->ret != STATUS_SUCCESS)
		_kern_thread_exit(-1);

	/* Save our ID. */
	curr_thread_id = _kern_thread_id(THREAD_SELF);

	entry->func(entry->arg);
	kern_thread_exit(0);
}

/** Create a new thread.
 * @param name		Name of the thread to create.
 * @param entry		Details of the entry point and stack for the new thread.
 *			See the documentation for thread_entry_t for details of
 *			the purpose of each member.
 * @param flags		Creation behaviour flags.
 * @param handlep	Where to store handle to the thread (can be NULL).
 * @return		Status code describing result of the operation. */
__export status_t kern_thread_create(const char *name,
	const thread_entry_t *entry, uint32_t flags, handle_t *handlep)
{
	thread_create_t create;
	thread_entry_t wrapper;
	status_t ret;

	if(!entry)
		return STATUS_INVALID_ARG;

	/* We need to call the above wrapper function to initialize the TLS
	 * block for the thread. */
	create.ret = STATUS_SUCCESS;
	create.entry = entry;
	wrapper.func = thread_entry_wrapper;
	wrapper.arg = &create;
	wrapper.stack = entry->stack;
	wrapper.stack_size = entry->stack_size;

	kern_mutex_lock(&thread_create_lock, -1);

	/* Create the thread. */
	ret = _kern_thread_create(name, &wrapper, flags, handlep);
	if(ret != STATUS_SUCCESS) {
		kern_mutex_unlock(&thread_create_lock);
		return ret;
	}

	/* Our mutex implementation is a simple one which does not take thread
	 * ownership into account. Therefore, to wait for the thread to signal
	 * that it's initialised, we just attempt to lock the mutex again, which
	 * will cause us to block. The new thread unlocks it once it is ready
	 * (see above), which will unblock us. */
	kern_mutex_lock(&thread_create_lock, -1);
	kern_mutex_unlock(&thread_create_lock);
	return create.ret;
}

/** Get the ID of a thread.
 * @param handle	Handle for thread to get ID of, or THREAD_SELF to get
 *			ID of the calling thread.
 * @return		Thread ID on success, -1 if handle is invalid. */
__export thread_id_t kern_thread_id(handle_t handle) {
	/* We save the current thread ID to avoid having to perform a kernel
	 * call just to get our own ID. */
	if(handle < 0) {
		return curr_thread_id;
	} else {
		return _kern_thread_id(handle);
	}
}

/** Terminate the calling thread.
 * @param status	Exit status code. */
__export void kern_thread_exit(int status) {
	tls_destroy();
	_kern_thread_exit(status);
}
