/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
