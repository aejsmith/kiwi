/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		POSIX type definitions.
 *
 * @todo		I'm not sure if useconds_t should be defined here. It
 *			is not mentioned at all by POSIX-2008, however it is in
 *			2004. If it is needed, it could be a problem, because
 *			the kernel defines a useconds_t which is signed.
 */

#ifndef __SYS_TYPES_H
#define __SYS_TYPES_H

#define __need_size_t
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** POSIX type definitions. */
typedef int32_t ssize_t;		/**< Used for a count of bytes or error indiction. */
typedef int64_t time_t;			/**< Used for UNIX timestamps. */
typedef int64_t clock_t;		/**< Used to store clock ticks since process start. */
typedef int32_t pid_t;			/**< Used to store a POSIX process ID. */
typedef int64_t off_t;			/**< Used for file sizes/offsets. */
typedef uint32_t mode_t;		/**< Used to store file attributes. */
typedef int64_t suseconds_t;		/**< Used to store a number of microseconds. */
typedef int32_t blkcnt_t;		/**< Used to store a count of blocks. */
typedef int32_t blksize_t;		/**< Used to store the size of a block. */
typedef uint32_t dev_t;			/**< Used to store a device number. */
typedef uint64_t ino_t;			/**< Used to store a filesystem node number. */
typedef uint32_t nlink_t;		/**< Used to store a number of blocks. */
typedef uint32_t uid_t;			/**< Used to store a user ID. */
typedef uint32_t gid_t;			/**< Used to store a group ID. */

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
/* pthread*_t */
/* timer_t */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TYPES_H */
