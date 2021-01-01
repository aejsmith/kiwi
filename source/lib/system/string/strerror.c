/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Error string function.
 */

#include <errno.h>
#include <string.h>

#include "libsystem.h"

/** Get string representation of an error number.
 * @param err           Error number.
 * @return              Pointer to string (should NOT be modified). */
char *strerror(int err) {
    if ((size_t)err >= __errno_count || !__errno_list[err])
        return (char *)"Unknown error";

    return (char *)__errno_list[err];
}

/** Get string representation of an error number.
 * @param err           Error number.
 * @param buf           Buffer in which to store error string.
 * @param buflen        Length of buffer.
 * @return              Pointer to string (should NOT be modified). */
int strerror_r(int err, char *buf, size_t buflen) {
    if ((size_t)err >= __errno_count || !__errno_list[err])
        return EINVAL;

    strncpy(buf, __errno_list[err], buflen);
    buf[buflen - 1] = 0;
    return 0;
}
