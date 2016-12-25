/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief               C library startup code.
 */

#include <core/path.h>

#define __KERNEL_PRIVATE
#include <kernel/private/process.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libsystem.h"

/** Name of the current program. */
const char *__program_name;

/** Early system library initialisation (run in .init). */
static __sys_init void libsystem_early_init(void) {
    /* Attempt to open standard I/O streams from existing handles. */
    stdin = fdopen(STDIN_FILENO, "r");
    stdout = fdopen(STDOUT_FILENO, "a");
    stderr = fdopen(STDERR_FILENO, "a");
}

/** System library initialisation function.
 * @param args          Process arguments structure. */
void libsystem_init(process_args_t *args) {
    /* Save the environment pointer. */
    environ = args->env;

    /* Save the program name. */
    __program_name = core_path_basename(args->path);

    /* If we're process 1, set default environment variables. */
    if (kern_process_id(-1) == 1) {
        setenv("PATH", "/system/bin", 1);
        setenv("HOME", "/users/admin", 1);
    }

    /* Call the main function. */
    exit(main(args->arg_count, args->args, args->env));
}
