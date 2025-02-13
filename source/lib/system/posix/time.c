/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX time functions.
 */

#include <core/time.h>

#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/time.h>

#include <sys/time.h>

#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "posix/posix.h"

/** Gets the current time.
 * @param tv            Structure to fill with time since epoch.
 * @param tz            Pointer to timezone (ignored).
 * @return              0 on success, -1 on failure. */
int gettimeofday(struct timeval *tv, void *tz) {
    nstime_t ktime;
    kern_time_get(TIME_REAL, &ktime);

    tv->tv_sec  = ktime / 1000000000;
    tv->tv_usec = (ktime % 1000000000) / 1000;

    return 0;
}

/** High resolution sleep.
 * @param rqtp          Requested sleep time.
 * @param rmtp          Where to store remaining time if interrupted.
 * @return              0 on success, -1 on failure. */
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    if (!rqtp || rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000) {
        errno = EINVAL;
        return -1;
    }

    nstime_t ns = nstime_from_timespec(rqtp);

    nstime_t rem;
    status_t ret = kern_thread_sleep(ns, &rem);
    if (ret == STATUS_INTERRUPTED) {
        if (rmtp)
            nstime_to_timespec(rem, rmtp);

        errno = EINTR;
        return -1;
    }

    return 0;
}

/** Sleep for a certain interval.
 * @param secs          Number of seconds to sleep for.
 * @return              0, or number of seconds remaining if interrupted. */
unsigned int sleep(unsigned int secs) {
    struct timespec ts;
    ts.tv_sec  = secs;
    ts.tv_nsec = 0;

    if (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        return ts.tv_sec;

    return 0;
}

/** Sleep for a certain interval.
 * @param usecs         Number of microseconds to sleep for.
 * @return              0 on success, -1 on failure. */
int usleep(useconds_t usecs) {
    status_t ret = kern_thread_sleep(core_usecs_to_nsecs(usecs), NULL);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Retrieve the time of the specified clock.
 * @param clock_id      Clock ID.
 * @param tp            Structure to fill with the results.
 * @return              0 on success, -1 on failure. */
int clock_gettime(clockid_t clock_id, struct timespec *tp) {
    if (!tp) {
        errno = EINVAL;
        return -1;
    }

    unsigned source;

    switch (clock_id) {
        case CLOCK_MONOTONIC:
            source = TIME_SYSTEM;
            break;
        case CLOCK_REALTIME:
            source = TIME_REAL;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    nstime_t ktime;
    status_t ret = kern_time_get(source, &ktime);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    nstime_to_timespec(ktime, tp);
    return 0;
}
