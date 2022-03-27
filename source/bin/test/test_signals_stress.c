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
 * @brief               Signals stress test.
 * 
 * This is a test using signals that is a great stress test of kernel thread
 * synchronisation, interrupt handling and IPC. Running it in a while true
 * loop from bash helped to flush out a bunch of issues.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void signal_handler(int num, siginfo_t *info, void *context) {
    printf("- Signal handler (num: %d, code: %d, pid: %d)\n", num, info->si_code, info->si_pid);
}

static void child_process(void) {
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

    printf("Raise complete\n");
}

int main(int argc, char **argv) {
    int pid = fork();
    if (pid == 0) {
        child_process();
        return EXIT_SUCCESS;
    } else if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    int ret = kill(pid, SIGTERM);
    if (ret != 0) {
        perror("kill");
        return EXIT_FAILURE;
    }

    int status;
    waitpid(-1, &status, 0);
    printf("Exited with status 0x%x\n", status);

    return EXIT_SUCCESS;
}
