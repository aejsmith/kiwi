/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               C library startup code.
 */

#include <core/path.h>

#include <kernel/process.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* This is here as a hack to make sure a link for this exists in the build tree
 * before libc++ is built (which include_next's it), because we don't have any
 * of wchar.h implemented yet and nothing else includes it. */
#include <wchar.h>
#include <wctype.h>

#include "libsystem.h"

/** Name of the current program. */
const char *__program_name;

/** Environment initialisation. */
static __sys_init_prio(LIBSYSTEM_INIT_PRIO_ARGS) void args_init(void) {
    const process_args_t *args = kern_process_args();

    environ = args->env;
    __program_name = core_path_basename(args->path);
}

/** Early stdio initialisation. */
static __sys_init_prio(LIBSYSTEM_INIT_PRIO_STDIO) void stdio_init(void) {
    /* Attempt to open standard I/O streams from existing handles. */
    stdin  = fdopen(STDIN_FILENO, "r");
    stdout = fdopen(STDOUT_FILENO, "a");
    stderr = fdopen(STDERR_FILENO, "a");
}

/** System library main function. */
void libsystem_main(void) {
    const process_args_t *args = kern_process_args();
    exit(main(args->arg_count, args->args, args->env));
}
