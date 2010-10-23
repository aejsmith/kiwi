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
 * @brief		C library startup code.
 */

#include <kernel/process.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libc.h"
#include "../kernel/libkernel.h"

/** Early C library initialisation. */
static void __attribute__((constructor)) libc_early_init(void) {
	/* Tell libkernel to use our allocation functions. */
	libkernel_heap_ops(malloc, free);

	/* Attempt to open standard I/O streams from existing handles. */
	stdin = fdopen(STDIN_FILENO, "r");
	stdout = fdopen(STDOUT_FILENO, "a");
	stderr = fdopen(STDERR_FILENO, "a");
}

/** C library initialisation function.
 * @param args		Process arguments structure. */
void libc_init(process_args_t *args) {
	/* Save the environment pointer. */
	environ = args->env;

	/* If we're process 1, set default environment variables. */
	if(process_id(-1) == 1) {
		setenv("PATH", "/system/binaries", 1);
		setenv("HOME", "/", 1);
	}

	/* Call the main function. */
	exit(main(args->args_count, args->args, args->env));
}
