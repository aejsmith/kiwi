/*
 * Copyright (C) 2009-2023 Alex Smith
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

static void mask(int how, int num) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, num);

    int ret = sigprocmask(how, &set, NULL);
    if (ret != 0)
        perror("sigprocmask");
}

static void child_process_default(void) {
    printf("Test default handler\n");

    while (true) {
        printf("- Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(2), NULL);
    }
}

static void child_process_default_mask(void) {
    printf("Test default handler with mask\n");

    mask(SIG_BLOCK, SIGTERM);

    while (true) {
        printf("- Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(2), NULL);

        printf("- Unblocking\n");
        mask(SIG_UNBLOCK, SIGTERM);
    }
}

static volatile bool signal_received;

static void signal_handler(int num, siginfo_t *info, void *context) {
    printf("- Signal handler (num: %d, code: %d, pid: %d)\n", num, info->si_code, info->si_pid);
    signal_received = true;
}

static void child_process_custom(void) {
    printf("Test custom handler\n");

    signal_received = false;

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
        kern_thread_sleep(core_secs_to_nsecs(2), NULL);
    }
}

static void child_process_custom_mask(void) {
    printf("Test custom handler with mask\n");

    signal_received = false;

    sigaction_t action = {};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = signal_handler;

    int ret = sigaction(SIGTERM, &action, NULL);
    if (ret != 0) {
        perror("sigaction");
        return;
    }

    mask(SIG_BLOCK, SIGTERM);

    while (!signal_received) {
        printf("- Child running\n");
        kern_thread_sleep(core_secs_to_nsecs(2), NULL);

        printf("- Unblocking\n");
        mask(SIG_UNBLOCK, SIGTERM);
    }
}

static void child_process_raise(void) {
    printf("Test raise()\n");

    sigaction_t action = {};
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = signal_handler;

    int ret = sigaction(SIGTERM, &action, NULL);
    if (ret != 0) {
        perror("sigaction");
        return;
    }

    ret = raise(SIGTERM);
    if (ret != 0) {
        perror("raise");
        return;
    }

    printf("- Raise complete\n");
}

static void child_process_exception(void) {
    printf("Test exception handler\n");

    mask(SIG_BLOCK, SIGTERM);

    /* Reset handler after first execution so we get an exception again and exit. */
    sigaction_t action = {};
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    action.sa_sigaction = signal_handler;

    int ret = sigaction(SIGILL, &action, NULL);
    if (ret != 0) {
        perror("sigaction");
        return;
    }

    __asm__ volatile("ud2a");

    printf("Shouldn't get here!");
}

static void (*test_functions[])() = {
    child_process_default,
    child_process_default_mask,
    child_process_custom,
    child_process_custom_mask,
    child_process_raise,
    child_process_exception,
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
