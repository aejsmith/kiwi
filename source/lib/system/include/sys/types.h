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
 * @brief               POSIX type definitions.
 */

#pragma once

#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_pid_t
#define __NEED_off_t
#define __NEED_mode_t
#define __NEED_suseconds_t
#define __NEED_useconds_t
#define __NEED_blkcnt_t
#define __NEED_blksize_t
#define __NEED_dev_t
#define __NEED_ino_t
#define __NEED_nlink_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_clockid_t
#include <bits/alltypes.h>

#include <system/pthread.h>

#include <stdint.h>

__SYS_EXTERN_C_BEGIN

/** Other type definitions. */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

/* [XSI] fsblkcnt_t */
/* [XSI] fsfilcnt_t */
/* id_t */
/* [XSI] key_t */
/* timer_t */

__SYS_EXTERN_C_END
