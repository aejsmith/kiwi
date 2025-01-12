/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Execute shell command function.
 */

#include <stdlib.h>
#include <unistd.h>

/** Execute a shell command.
 * @param command       Command line to execute, will be run using 'sh -c <line>'.
 * @return              Exit status of process (in format returned by wait()),
 *                      or -1 if unable to fork the process. */
int system(const char *command) {
    int status;
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        execl("/system/bin/sh", "/system/bin/sh", "-c", command, NULL);
        exit(127);
    } else if (pid > 0) {
        pid = waitpid(pid, &status, 0);
        if (pid < 0)
            return -1;

        return status;
    } else {
        return -1;
    }
}
