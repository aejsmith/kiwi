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
