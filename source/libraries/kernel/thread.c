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
 * @brief		Thread functions.
 */

#include <kernel/object.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>

#include "libkernel.h"

/** Information used by thread_create(). */
typedef struct thread_create_info {
	handle_t sem;			/**< Semaphore for communication between entry and create. */
	status_t ret;			/**< Initialisation status. */
	void (*func)(void *);		/**< Real entry function. */
	void *arg;			/**< Argument to entry function. */
} thread_create_info_t;

extern status_t _kern_thread_create(const char *name, void *stack, size_t stacksz, void (*func)(void *),
                                   void *arg, const object_security_t *security, object_rights_t rights,
                               handle_t *handlep);
extern void _kern_thread_exit(int status) __attribute__((noreturn));

/** Thread entry wrapper.
 * @param arg		Pointer to information structure. */
static void thread_entry_wrapper(void *arg) {
	thread_create_info_t *info = arg;
	void (*func)(void *);

	/* Attempt to initialise our TLS block. */
	info->ret = tls_init();
	if(info->ret != STATUS_SUCCESS) {
		_kern_thread_exit(-1);
	}

	func = info->func;
	arg = info->arg;
	kern_semaphore_up(info->sem, 1);

	func(arg);
	kern_thread_exit(0);
}

/** Create a new thread.
 * @param name		Name of the thread to create.
 * @param stack		Pointer to base of stack to use for thread. If NULL,
 *			then a new stack will be allocated.
 * @param stacksz	Size of stack. If a stack is provided, then this should
 *			be the size of that stack. Otherwise, it is used as the
 *			size of the stack to create - if it is zero then a
 *			stack of the default size will be allocated.
 * @param func		Function to execute.
 * @param arg		Argument to pass to thread.
 * @param handlep	Where to store handle to the thread (can be NULL).
 * @return		Status code describing result of the operation. */
__export status_t kern_thread_create(const char *name, void *stack, size_t stacksz, void (*func)(void *),
                                     void *arg, const object_security_t *security, object_rights_t rights,
                                     handle_t *handlep) {
	thread_create_info_t info;
	status_t ret;

	/* Create the semaphore that we use to wait for the thread to signal
	 * that its initialisation has completed. */
	ret = kern_semaphore_create("thread_create_sem", 0, NULL, SEMAPHORE_USAGE, &info.sem);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	info.ret = STATUS_SUCCESS;
	info.func = func;
	info.arg = arg;

	ret = _kern_thread_create(name, stack, stacksz, thread_entry_wrapper, &info, security, rights, handlep);
	if(ret != STATUS_SUCCESS) {
		kern_handle_close(info.sem);
		return ret;
	}

	/* Wait for the thread to signal that it is initialised. */
	kern_semaphore_down(info.sem, -1);
	kern_handle_close(info.sem);
	return info.ret;
}

/** Terminate the calling thread.
 * @param status	Exit status code. */
__export void kern_thread_exit(int status) {
	tls_destroy();
	_kern_thread_exit(status);
}
