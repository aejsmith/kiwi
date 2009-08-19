/* Kiwi C library - Exit functions.
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
 * @brief		Exit functions.
 */

#include <kernel/process.h>

#include <stdlib.h>

typedef void (*atexit_t)(void);

static atexit_t atexit_functions[ATEXIT_MAX];
static int atexit_count = 0;

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
	if(atexit_count >= ATEXIT_MAX) {
		return -1;
	}

	atexit_functions[atexit_count++] = function;
	return 0;
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
	int i;

	/* Run atexit functions */
	for(i = 0; i < atexit_count; i++) {
		atexit_functions[i]();
	}

	process_exit(status);
}

/** Terminate execution.
 *
 * Terminates execution without calling at-exit functions. Equivalent to
 * _exit().
 *
 * @param status	Exit status.
 *
 * @return		Does not return.
 */
void _Exit(int status) {
	process_exit(status);
}
