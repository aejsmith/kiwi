/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Current time function.
 */

#include <sys/time.h>

#include <time.h>

/** Get the current time as seconds since the UNIX epoch.
 * @param timep         If not NULL, result will also be stored here.
 * @return              Current time. */
time_t time(time_t *timep) {
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return -1;

    if (timep)
        *timep = tv.tv_sec;

    return tv.tv_sec;
}
