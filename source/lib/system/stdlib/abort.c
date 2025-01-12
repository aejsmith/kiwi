/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Abort function.
 */

#include <kernel/process.h>
#include <kernel/thread.h>

#include <stdlib.h>

/** Abort program execution. */
void abort(void) {
    /*
     * This function must always terminate the program. We must also respect
     * POSIX SIGABRT configuration. The way to do this is:
     *  1. Raise an EXCEPTION_ABORT exception.
     *     a. If a native exception handler is installed, that would be called.
     *     b. If a POSIX SIGABRT handler is registered, that would be called
     *        through the POSIX exception handler. This will happen even if
     *        SIGABRT is masked - the signal mask is ignored for exceptions.
     *  2. If that returns, then the handler returned. In that case, we
     *     forcibly override the exception handlers to NULL, and try again.
     *  3. If that returns, something could have come in on another thread and
     *     installed an exception handler again. In that case, we just do a
     *     normal exit as a last resort.
     */

    exception_info_t info;
    info.code = EXCEPTION_ABORT;
    kern_thread_exception(&info);

    kern_process_set_exception_handler(EXCEPTION_ABORT, NULL);
    kern_thread_set_exception_handler(EXCEPTION_ABORT, NULL);
    kern_thread_exception(&info);

    kern_process_exit(-1);
}
