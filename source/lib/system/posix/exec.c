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
 * @brief               POSIX program execution functions.
 */

#include <kernel/process.h>
#include <kernel/status.h>

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "posix/posix.h"

#define ARGV_MAX    512
#define INTERP_MAX  256

extern char **environ;

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

/**
 * Executes a binary with the given arguments and a copy of the calling
 * process' environment.
 *
 * @param path          Path to binary to execute.
 * @param argv          Arguments for process (NULL-terminated array).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

/**
 * Executes a binary with PATH lookup. If the given path contains a / character,
 * this function will simply call execve() with the given arguments and the
 * current process' environment. Otherwise, it will search the PATH for the
 * name given and execute it if found.
 *
 * @param file          Name of binary to execute.
 * @param argv          Arguments for process (NULL-terminated array).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execvp(const char *file, char *const argv[]) {
    /* If file contains a /, just run it */
    if (strchr(file, '/'))
        return execve(file, argv, environ);

    /* Use the default path if PATH is not set in the environment */
    const char *path = getenv("PATH");
    if (!path)
        path = "/system/bin";

    char buf[PATH_MAX];

    for (char *cur = (char *)path, *next; cur; cur = next) {
        next = strchr(cur, ':');
        if (!next)
            next = cur + strlen(cur);

        if (next == cur) {
            buf[0] = '.';
            cur--;
        } else {
            if ((next - cur) >= (PATH_MAX - 3)) {
                errno = EINVAL;
                return -1;
            }

            memmove(buf, cur, (size_t)(next - cur));
        }

        buf[next - cur] = '/';

        size_t len = strlen(file);
        if (len + (next - cur) >= (PATH_MAX - 2)) {
            errno = EINVAL;
            return -1;
        }

        memmove(&buf[next - cur + 1], file, len + 1);
        if (execve(buf, argv, environ) == -1) {
            if ((errno != EACCES) && (errno != ENOENT) && (errno != ENOTDIR))
                return -1;
        }

        if (!*next)
            break;

        next++;
    }

    return -1;
}

/**
 * Executes a binary with the given arguments and a copy of the calling
 * process' environment.
 *
 * @param path          Path to binary to execute.
 * @param arg...        Arguments for process (NULL-terminated argument list).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execl(const char *path, const char *arg, ...) {
    const char *argv[ARGV_MAX];
    argv[0] = arg;

    va_list ap;
    va_start(ap, arg);

    for (unsigned i = 1; i < ARGV_MAX; i++) {
        argv[i] = va_arg(ap, const char *);
        if (!argv[i])
            break;
    }

    va_end(ap);
    return execv(path, (char *const *)argv);
}

/**
 * Executes a binary with PATH lookup. If the given path contains a / character,
 * this function will simply call execve() with the given arguments and the
 * current process' environment. Otherwise, it will search the PATH for the
 * name given and execute it if found.
 *
 * @param file          Name of binary to execute.
 * @param arg...        Arguments for process (NULL-terminated argument list).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execlp(const char *file, const char *arg, ...) {
    const char *argv[ARGV_MAX];
    argv[0] = arg;

    va_list ap;
    va_start(ap, arg);

    for (unsigned i = 1; i < ARGV_MAX; i++) {
        argv[i] = va_arg(ap, const char *);
        if (!argv[i])
            break;
    }

    va_end(ap);
    return execvp(file, (char *const *)argv);
}
