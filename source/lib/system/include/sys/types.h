/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               POSIX type definitions.
 */

#pragma once

#include <kernel/types.h>

#include <system/pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** POSIX type definitions. */
typedef int64_t time_t;             /**< Used for UNIX timestamps. */
typedef int64_t clock_t;            /**< Used to store clock ticks since process start. */
typedef int32_t pid_t;              /**< Used to store a POSIX process ID. */
typedef int64_t off_t;              /**< Used for file sizes/offsets. */
typedef uint32_t mode_t;            /**< Used to store file attributes. */
typedef int64_t suseconds_t;        /**< Used to store a (signed) number of microseconds. */
typedef uint64_t useconds_t;        /**< Used to store a number of microseconds. */
typedef int32_t blkcnt_t;           /**< Used to store a count of blocks. */
typedef int32_t blksize_t;          /**< Used to store the size of a block. */
typedef uint32_t dev_t;             /**< Used to store a device number. */
typedef uint64_t ino_t;             /**< Used to store a filesystem node number. */
typedef uint32_t nlink_t;           /**< Used to store a number of blocks. */
typedef uint32_t uid_t;             /**< Used to store a user ID. */
typedef uint32_t gid_t;             /**< Used to store a group ID. */

/** Other type definitions. */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

/* clockid_t */
/* [XSI] fsblkcnt_t */
/* [XSI] fsfilcnt_t */
/* id_t */
/* [XSI] key_t */
/* timer_t */

#ifdef __cplusplus
}
#endif
