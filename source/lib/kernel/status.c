/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Status string function.
 */

#include <kernel/status.h>

extern const char *__kern_status_strings[];
extern size_t __kern_status_size;

/** Get a string from a kernel status code.
 * @param status        Status code.
 * @return              String pointer (returns pointer to "<unknown>" if not
 *                      recognised). */
__sys_export const char *kern_status_string(status_t status) {
    if ((size_t)status >= __kern_status_size || !__kern_status_strings[status])
        return "<unknown>";

    return __kern_status_strings[status];
}
