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
#include <core/utility.h>

#include <kernel/thread.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void child_process_default(void) {
    printf("Test default handler\n");

    while (true) {
        printf("- Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(1), NULL);
    }
}

static volatile bool signal_received = false;

static void signal_handler(int num, siginfo_t *info, void *context) {
    printf("- Signal handler (num: %d, pid: %d)\n", num, info->si_pid);
    signal_received = true;
}

static void child_process_custom(void) {
    printf("Test custom handler\n");

    sigaction_t action = {};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = signal_handler;

    int ret = sigaction(SIGTERM, &action, NULL);
    if (ret != 0) {
        perror("sigaction");
        return;
    }

    while (!signal_received) {
        printf("- Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(1), NULL);
    }
}

static void (*test_functions[])() = {
    child_process_default,
    child_process_custom,
};

int main(int argc, char **argv) {
    for (size_t i = 0; i < core_array_size(test_functions); i++) {
        int pid = fork();
        if (pid == 0) {
            test_functions[i]();
            return EXIT_SUCCESS;
        } else if (pid == -1) {
            perror("fork");
            return EXIT_FAILURE;
        }

        kern_thread_sleep(core_msecs_to_nsecs(500), NULL);

        int ret = kill(pid, SIGTERM);
        if (ret != 0) {
            perror("kill");
            return EXIT_FAILURE;
        }

        int status;
        waitpid(-1, &status, 0);
        printf("Exited with status 0x%x\n", status);
    }

    //kill(getpid(), SIGTERM);
    //printf("Why am I here?\n");
    return EXIT_SUCCESS;
}
