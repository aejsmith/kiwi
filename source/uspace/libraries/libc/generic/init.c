/* Kiwi C library - Initialization code
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
 * @brief		Initialization code.
 */

#include <kernel/handle.h>
#include <kernel/process.h>

#define __need_NULL
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stdio/stdio_priv.h"

extern void kputch(char ch);
extern void __libc_init(process_args_t *args);
extern int main(int argc, char **argv, char **envp);
extern void *__gxx_personality_v0;
extern char **environ;

void *__gxx_personality_v0 = NULL;

FILE *stdin;
FILE *stdout;
FILE *stderr;

/** Output a string to the kernel console.
 * @param str		String to output. */
static void kwrite(const char *str) {
	size_t i;

	for(i = 0; i < strlen(str); i++) {
		kputch(str[i]);
	}
}

/** C library initialization function.
 * @param args		Process arguments structure. */
void __libc_init(process_args_t *args) {
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
                kwrite("*** libc fatal: could not open stdio streams\n");
                process_exit(1);
        }

	process_exit(main(args->args_count, args->args, args->env));
}
