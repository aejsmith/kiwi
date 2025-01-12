/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX pipe creation function.
 */

#include <kernel/pipe.h>
#include <kernel/status.h>

#include <unistd.h>

#include "libsystem.h"

/** Create an interprocess channel.
 * @param fds           Where to store file descriptors to each end of pipe.
 * @return              0 on success, -1 on failure. */
int pipe(int fds[2]) {
    status_t ret = kern_pipe_create(0, 0, &fds[0], &fds[1]);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}
