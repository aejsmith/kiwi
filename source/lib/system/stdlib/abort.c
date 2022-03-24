/*
 * Copyright (C) 2009-2022 Alex Smith
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
