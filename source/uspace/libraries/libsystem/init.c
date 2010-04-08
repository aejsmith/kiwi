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
 * @brief		Userspace application startup code.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libsystem.h"
#include "stdio/stdio_priv.h"

/** Preallocated file structures to use for standard I/O. */
FILE __stdin, __stdout, __stderr;

/** Standard input/output streams. */
FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

/** Userspace application initialisation function.
 * @param args		Process arguments structure. */
void __libsystem_init(process_args_t *args) {
	const char *console;

	environ = args->env;
	console = getenv("CONSOLE");

	/* Attempt to open streams from existing handles, and open new streams
	 * if we can't. */
	if(!fopen_handle(0, stdin)) {
		if(!console || !fopen_device(console, stdin)) {
			fopen_kconsole(stdin);
		}
	}
	if(!fopen_handle(1, stdout)) {
		if(!console || !fopen_device(console, stdout)) {
			fopen_kconsole(stdout);
		}
	}
	if(!fopen_handle(2, stderr)) {
		if(!console || !fopen_device(console, stderr)) {
			fopen_kconsole(stderr);
		}
	}

	exit(main(args->args_count, args->args, args->env));
}
