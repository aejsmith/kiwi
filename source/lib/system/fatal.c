/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Fatal error functions.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "stdio/stdio.h"

#include "libsystem.h"

/** Helper for libsystem_fatal(). */
static void libsystem_fatal_helper(char ch, void *data) {
    if (data)
        fputc(ch, (FILE *)data);
}

/** Print out a fatal error and terminate the process.
 * @param fmt           Format string.
 * @param ...           Arguments to substitute into format. */
void libsystem_fatal(const char *fmt, ...) {
    va_list args;

    do_printf(libsystem_fatal_helper, stderr, "%s: libsystem: fatal error: ", __program_name);

    va_start(args, fmt);
    do_vprintf(libsystem_fatal_helper, stderr, fmt, args);
    va_end(args);

    if (stderr)
        fputc('\n', stderr);

    abort();
}

/** Handle a call to a stub function.
 * @param name          Name of function.
 * @param fatal         Whether the error is considered fatal. */
void libsystem_stub(const char *name, bool fatal) {
    if (fatal) {
        libsystem_fatal("unimplemented function: %s", name);
    } else {
        libsystem_log(CORE_LOG_ERROR, "%s unimplemented", name);
        errno = ENOSYS;
    }
}

/** Print out an assertion fail message.
 * @param cond          Condition.
 * @param file          File it occurred in.
 * @param line          Line number.
 * @param func          Function name. */
void libsystem_assert_fail(const char *cond, const char *file, unsigned int line, const char *func) {
    libsystem_fatal("assertion '%s' failed at %s:%d (%s)", cond, file, line, func);
}

/** Print out an assertion fail message.
 * @param cond          Condition.
 * @param file          File it occurred in.
 * @param line          Line number.
 * @param func          Function name. */
void __assert_fail(const char *cond, const char *file, unsigned int line, const char *func) {
    if (!func) {
        fprintf(stderr, "Assertion '%s' failed at %s:%d\n", cond, file, line);
    } else {
        fprintf(stderr, "Assertion '%s' failed at %s:%d (%s)\n", cond, file, line, func);
    }

    abort();
}
