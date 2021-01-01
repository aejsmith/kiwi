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
 * @brief               POSIX file control functions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** File access mode flags for open(). */
#define O_RDONLY        0x0001      /**< Open for reading. */
#define O_WRONLY        0x0002      /**< Open for writing. */
#define O_RDWR          0x0003      /**< Open for reading and writing. */

/** Mask to get file access mode from open flags. */
#define O_ACCMODE       O_RDWR

/** File creation flags for open(). */
#define O_CREAT         0x0004      /**< Create file if it does not exist. */
#define O_EXCL          0x0008      /**< Exclusive use flag. */
//#define O_NOCTTY      0x0010      /**< Do not assign controlling terminal. */
#define O_TRUNC         0x0020      /**< Truncate flag. */

/** File status flags for open() and fcntl(). */
#define O_APPEND        0x0040      /**< File offset should be set to end before each write. */
#define O_NONBLOCK      0x0080      /**< Non-blocking I/O mode. */

/** Other flags for open(). */
#define O_CLOEXEC       0x0100      /**< Open with FD_CLOEXEC flag set. */
#define O_DIRECTORY     0x0200      /**< The call should fail if not a directory. */

/** File descriptor flags for fcntl(). */
#define FD_CLOEXEC      0x0001      /**< File should be closed on execve(). */

/** Action flags for fcntl(). */
#define F_DUPFD         1           /**< Duplicate file descriptor. */
#define F_DUPFD_CLOEXEC 2           /**< Duplicate file descriptor with FD_CLOEXEC set. */
#define F_GETFD         3           /**< Get file descriptor flags. */
#define F_SETFD         4           /**< Set file descriptor flags. */
#define F_GETFL         5           /**< Get file status flags and file access modes. */
#define F_SETFL         6           /**< Set file status flags. */
//#define F_GETLK       7           /**< Get record locking information. */
//#define F_SETLK       8           /**< Set record locking information. */
//#define F_SETLKW      9           /**< Set record locking information; wait if blocked. */
//#define F_GETOWN      10          /**< Get process or process group ID to receive SIGURG signals. */
//#define F_SETOWN      11          /**< Set process or process group ID to receive SIGURG signals. */

extern int creat(const char *path, mode_t mode);
extern int fcntl(int fd, int cmd, ...);
extern int open(const char *path, int oflag, ...);
/* int openat(int, const char *, int, ...); */

__SYS_EXTERN_C_END
