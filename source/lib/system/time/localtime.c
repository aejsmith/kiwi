/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Local time function.
 */

#include <time.h>

static struct tm __localtime_tm;

/**
 * Get the local time.
 *
 * Gets the local time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep         Time to use.
 * @param tm            Time structure to fill in.
 *
 * @return              Pointer to time structure filled in.
 */
struct tm *localtime_r(const time_t *restrict timep, struct tm *restrict tm) {
    return gmtime_r(timep, tm);
}

/**
 * Get the local time.
 *
 * Gets the local time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep         Time to use.
 *
 * @return              Pointer to time structure filled in.
 */
struct tm *localtime(const time_t *timep) {
    return localtime_r(timep, &__localtime_tm);
}
