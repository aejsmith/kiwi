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
 * @brief               POSIX signals test.
 */

#include <core/time.h>

#include <kernel/thread.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void child_process(void) {
    while (true) {
        printf("Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(1), NULL);
    }
}

int main(int argc, char **argv) {
    // Notes:
    //  - Need to change IPL to take posix_service_lock in case the signal is
    //    received while lock held by handler thread (will this be good enough?
    //    what is the behaviour of receiving callback while IPL blocks it).
    //  - Need security context send support in core_connection.
    //  - Need to handle exec behaviour somehow - ignored signals (except
    //    SIGCHLD) should be inherited.

    int pid = fork();
    if (pid == 0) {
        child_process();
        return EXIT_SUCCESS;
    } else if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    kern_thread_sleep(core_msecs_to_nsecs(500), NULL);

    int ret = kill(pid, SIGTERM);
    if (ret != 0)
        perror("kill");

    waitpid(-1, NULL, 0);
    return EXIT_SUCCESS;
}
