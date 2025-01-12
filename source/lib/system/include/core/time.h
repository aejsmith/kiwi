/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Time functions.
 */

#pragma once

#include <kernel/time.h>

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

/** Convert seconds to nanoseconds.
 * @param secs          Seconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t core_secs_to_nsecs(nstime_t secs) {
    return secs * 1000000000;
}

/** Convert milliseconds to nanoseconds.
 * @param msecs         Milliseconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t core_msecs_to_nsecs(nstime_t msecs) {
    return msecs * 1000000;
}

/** Convert microseconds to nanoseconds.
 * @param usecs         Microseconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t core_usecs_to_nsecs(nstime_t usecs) {
    return usecs * 1000;
}

/** Convert nanoseconds to seconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in seconds. */
static inline nstime_t core_nsecs_to_secs(nstime_t nsecs) {
    return nsecs / 1000000000;
}

/** Convert nanoseconds to milliseconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in milliseconds. */
static inline nstime_t core_nsecs_to_msecs(nstime_t nsecs) {
    return nsecs / 1000000;
}

/** Convert nanoseconds to microseconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in microseconds. */
static inline nstime_t core_nsecs_to_usecs(nstime_t nsecs) {
    return nsecs / 1000;
}

__SYS_EXTERN_C_END
