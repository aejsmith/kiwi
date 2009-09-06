/* Userspace application startup code
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

void *__gxx_personality_v0 = NULL;

/** Standard input/output streams. */
FILE *stdin = NULL;
FILE *stdout = NULL;
FILE *stderr = NULL;

/** Userspace application initialisation function.
 * @param args		Process arguments structure. */
void __libsystem_init(process_args_t *args) {
	const char *console;

	environ = args->env;
	console = getenv("CONSOLE");

	/* Attempt to open streams from existing handles, and open new streams
	 * if we can't. */
	if(!(stdin = fopen_handle(0))) {
		if(!(stdin = fopen_device(console))) {
			stdin = fopen_kconsole();
		}
	}
	if(!(stdout = fopen_handle(1))) {
		if(!(stdout = fopen_device(console))) {
			stdout = fopen_kconsole();
		}
	}
	if(!(stderr = fopen_handle(2))) {
		if(!(stderr = fopen_device(console))) {
			stderr = fopen_kconsole();
		}
	}

        if(!stdin || !stdout || !stderr) {
                __libsystem_fatal("Could not open stdio streams");
        }

	process_exit(main(args->args_count, args->args, args->env));
}
