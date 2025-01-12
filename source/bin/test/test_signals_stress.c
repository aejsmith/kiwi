/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
