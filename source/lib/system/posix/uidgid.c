/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX user/group functions.
 */

#include "libsystem.h"

#include <kernel/process.h>
#include <kernel/security.h>
#include <kernel/status.h>

#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

/** Get the process' effective group ID.
 * @return              Effective group ID of the process. */
gid_t getegid(void) {
    security_context_t ctx;
    status_t ret = kern_process_security(PROCESS_SELF, &ctx);
    if (ret != STATUS_SUCCESS)
        libsystem_fatal("failed to obtain security context: %" PRId32, ret);

    return ctx.gid;
}

/** Get the process' effective user ID.
 * @return              Effective user ID of the process. */
uid_t geteuid(void) {
    security_context_t ctx;
    status_t ret = kern_process_security(PROCESS_SELF, &ctx);
    if (ret != STATUS_SUCCESS)
        libsystem_fatal("failed to obtain security context: %" PRId32, ret);

    return ctx.uid;
}

/** Get the process' group ID.
 * @return              Group ID of the process. */
gid_t getgid(void) {
    return getegid();
}

/** Get the process' user ID.
 * @return              User ID of the process. */
uid_t getuid(void) {
    return geteuid();
}

/** Set the group ID of the process.
 * @param gid           Group ID to set.
 * @return              0 on success, -1 on failure. */
int setgid(gid_t gid) {
    libsystem_stub("setgid", false);
    return -1;
}

/** Set the user ID of the process.
 * @param uid           User ID to set.
 * @return              0 on success, -1 on failure. */
int setuid(uid_t uid) {
    libsystem_stub("setuid", false);
    return -1;
}
