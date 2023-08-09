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
