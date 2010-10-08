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
 * @brief		Exit functions.
 *
 * @todo		Need locking on the atexit array.
 */

#include <kernel/process.h>

#include <util/list.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "../libc.h"

extern void *__dso_handle __attribute__((__weak__));

extern int __cxa_atexit(void (*function)(void *), void *arg, void *dso);
extern void __cxa_finalize(void *d);

/** Structure defining an at-exit function. */
typedef struct atexit_func {
	list_t header;			/**< List header. */

	void (*func)(void *);		/**< Function pointer. */
	void *arg;			/**< Function argument. */
	void *dso;			/**< DSO handle. */
} atexit_func_t;

/** Statically allocated structures. */
static atexit_func_t __atexit_array[ATEXIT_MAX];
static bool __atexit_inited = false;

/** List of free at-exit functions. */
static LIST_DECLARE(__atexit_free_funcs);
static size_t __atexit_free_count = 0;

/** List of registered at-exit functions. */
static LIST_DECLARE(__atexit_funcs);
static size_t __atexit_count = 0;

/** Allocate an at-exit function structure.
 * @return		Function structure pointer, or NULL if none free. */
static atexit_func_t *__atexit_alloc(void) {
	atexit_func_t *func = NULL;
	size_t i;

	if(!__atexit_inited) {
		for(i = 0; i < ATEXIT_MAX; i++) {
			list_init(&__atexit_array[i].header);
			list_append(&__atexit_free_funcs, &__atexit_array[i].header);
			__atexit_free_count++;
		}

		__atexit_inited = true;
	}

	if(__atexit_free_count) {
		if(list_empty(&__atexit_free_funcs)) {
			libc_fatal("atexit data is corrupted");
		}

		func = list_entry(__atexit_free_funcs.next, atexit_func_t, header);
		list_remove(&func->header);
	}

	return func;
}

/** Free an at-exit function structure.
 * @param func		Function structure. */
static void __atexit_free(atexit_func_t *func) {
	list_append(&__atexit_free_funcs, &func->header);
	__atexit_free_count++;
}

/** Register a C++ cleanup function.
 * @param function	Function to call.
 * @param arg		Argument.
 * @param dso		DSO handle.
 * @return		0 on success, -1 on failure. */
int __cxa_atexit(void (*function)(void *), void *arg, void *dso) {
	atexit_func_t *func;

	if(!(func = __atexit_alloc())) {
		return -1;
	}

	func->func = (void (*)(void *))function;
	func->arg = arg;
	func->dso = dso;

	list_prepend(&__atexit_funcs, &func->header);
	__atexit_count++;
	return 0;
}

/** Run C++ cleanup functions.
 * @param d		DSO handle, if NULL will call all handlers. */
void __cxa_finalize(void *d) {
	atexit_func_t *func;
	size_t count;
restart:
	LIST_FOREACH_SAFE(&__atexit_funcs, iter) {
		func = list_entry(iter, atexit_func_t, header);
		count = __atexit_count;

		if(!d || d == func->dso) {
			func->func(func->arg);

			if(__atexit_count != count) {
				__atexit_free(func);
				goto restart;
			} else {
				__atexit_free(func);
			}
		}
	}
}

/** Define a function to run at process exit.
 *
 * Defines a function to be run at normal (i.e. invocation of exit())
 * process termination. Use of _exit() or _Exit(), or involuntary process
 * termination, will not result in functions registered with this function
 * being called.
 *
 * @param function	Function to define.
 *
 * @return		0 on success, -1 on failure.
 */
int atexit(void (*function)(void)) {
	return __cxa_atexit((void (*)(void *))function, NULL, (&__dso_handle) ? __dso_handle : NULL);
}

/** Call at-exit functions and terminate execution.
 *
 * Calls all functions previously defined with atexit() and terminates
 * the process.
 *
 * @param status	Exit status.
 *
 * @return		Does not return.
 */
void exit(int status) {
	__cxa_finalize(NULL);
	process_exit(status);
}

/** Terminate execution without calling at-exit functions.
 * @param status	Exit status.
 * @return		Does not return. */
void _exit(int status) {
	process_exit(status);
}

/** Terminate execution without calling at-exit functions.
 * @param status	Exit status.
 * @return		Does not return. */
void _Exit(int status) {
	process_exit(status);
}
