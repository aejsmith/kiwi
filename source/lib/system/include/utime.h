/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Access/modification time functions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** POSIX utimbuf structure. */
struct utimbuf {
    time_t actime;                  /**< Access time. */
    time_t modtime;                 /**< Modification time. */
};

extern int utime(const char *path, const struct utimbuf *times);

__SYS_EXTERN_C_END
