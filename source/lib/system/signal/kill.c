/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief               POSIX signal send functions.
 */

#include <errno.h>
#include <signal.h>

#include "libsystem.h"

/** Send a signal to a process.
 * @param pid           ID of process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int kill(pid_t pid, int num) {
    libsystem_stub("raise", true);
    return -1;
}

/** Send a signal to the current process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int raise(int num) {
    __asm__ volatile("ud2a");
    libsystem_stub("raise", true);
    return -1;
}
