/*
 * Copyright (C) 2008-2013 Alex Smith
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

#include <stdarg.h>
#include <unistd.h>

#define ARGV_MAX        512

/**
 * Execute a binary in the PATH.
 *
 * If the given path contains a / character, this function will simply call
 * execve() with the given arguments and the current process' environment.
 * Otherwise, it will search the PATH for the name given and execute it
 * if found.
 *
 * @param file          Name of binary to execute.
 * @param arg           Arguments for process (NULL-terminated argument list).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execlp(const char *file, const char *arg, ...) {
    int i;
    va_list ap;
    const char *argv[ARGV_MAX];

    argv[0] = arg;
    va_start(ap, arg);

    for (i = 1; i < ARGV_MAX; i++) {
        argv[i] = va_arg(ap, const char *);
        if (!argv[i])
            break;
    }

    va_end(ap);
    return execvp(file, (char *const *)argv);
}

/**
 * Execute a binary.
 *
 * Executes a binary with the given arguments and the current process'
 * environment.
 *
 * @param path          Path to binary to execute.
 * @param arg           Arguments for process (NULL-terminated argument list).
 *
 * @return              Does not return on success, -1 on failure.
 */
int execl(const char *path, const char *arg, ...) {
    int i;
    va_list ap;
    const char *argv[ARGV_MAX];

    argv[0] = arg;
    va_start(ap, arg);

    for (i = 1; i < ARGV_MAX; i++) {
        argv[i] = va_arg(ap, const char *);
        if (!argv[i])
            break;
    }

    va_end(ap);
    return execv(path, (char *const *)argv);
}
