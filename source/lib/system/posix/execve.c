/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               POSIX program execution function.
 */

#include <kernel/process.h>
#include <kernel/status.h>

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "posix/posix.h"

#define INTERP_MAX 256

/** Execute a file with an interpreter. */
static int do_interp(int fd, const char *path, char *const argv[], char *const envp[]) {
    char buf[INTERP_MAX];

    /* FD offset is already past '#!'. */
    ssize_t len = read(fd, buf, INTERP_MAX - 1);

    /* Not needed past this, don't want to leak into child. */
    close(fd);

    if (len < 0)
        return -1;

    buf[len] = 0;

    char *end = strchr(buf, '\n');
    if (!end) {
        /* Interpreter is too long. */
        errno = ENOEXEC;
        return -1;
    }

    end[0] = 0;

    /*
     * Find the path and optional argument. We follow Linux behaviour here: any
     * optional argument is passed as a single argument to the interpreter.
     * Whitespace preceding and following both the path and the argument is
     * stripped off. For example, for the following interpreter line:
     *
     *   "#!  /foo/bar    test1   test2    "
     *
     * We execute "/foo/bar", with it's first argument as "test1   test2", and
     * the original path as its second argument.
     */

    char *interp = buf;

    /* Skip whitespace preceding path. */
    while (isspace(interp[0]))
        interp++;

    char *arg = interp;

    /* Skip over path. */
    while (arg[0] && !isspace(arg[0]))
        arg++;

    /* Null-terminate path. */
    if (arg[0]) {
        arg[0] = 0;
        arg++;

        /* Skip whitespace preceding argument. */
        while (isspace(arg[0]))
            arg++;

        if (arg[0]) {
            /* Skip trailing whitespace. */
            while (isspace(end[-1]))
                end--;

            end[0] = 0;
        }
    }

    if (!interp[0]) {
        errno = ENOEXEC;
        return -1;
    }

    if (!arg[0])
        arg = NULL;

    /* Get original argument count (including null terminator). */
    int orig_argc = 1;
    while (argv[orig_argc - 1])
        orig_argc++;

    /* Build new argument array. Interpreter gets the path string, original
     * argv[0] is lost. */
    int new_argc = orig_argc + ((arg) ? 2 : 1);
    char **new_argv = malloc(sizeof(char *) * new_argc);
    if (!new_argv)
        return -1;

    new_argc = 0;
    new_argv[new_argc++] = interp;
    if (arg)
        new_argv[new_argc++] = arg;

    new_argv[new_argc++] = (char *)path;
    for (int i = 1; i < orig_argc; i++)
        new_argv[new_argc++] = argv[i];

    /* Recurse to handle the interpreter itself also requiring an interpreter. */
    int ret = execve(interp, new_argv, envp);
    free(new_argv);
    return ret;
}

/**
 * Executes a binary with the given arguments and a copy of the provided
 * environment block.
 *
 * @param path          Path to binary to execute.
 * @param argv          Arguments for process (NULL-terminated array).
 * @param envp          Environment for process (NULL-terminated array).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execve(const char *path, char *const argv[], char *const envp[]) {
    if (access(path, X_OK) != 0)
        return -1;

    /* Open the file and check if it is an interpreter. */
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[2];
    if (read(fd, buf, 2) == 2 && buf[0] == '#' && buf[1] == '!')
        return do_interp(fd, path, argv, envp);

    close(fd);

    /* If this returns it must have failed. */
    status_t ret = kern_process_exec(path, (const char *const *)argv, (const char *const *)envp, 0, NULL);
    libsystem_status_to_errno(ret);
    return -1;
}
