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
 * @brief		POSIX file control functions.
 */

#ifndef __FCNTL_H
#define __FCNTL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** File access mode flags for open(). */
#define O_RDONLY	0x0001		/**< Open for reading. */
#define O_WRONLY	0x0002		/**< Open for writing. */
#define O_RDWR		0x0003		/**< Open for reading and writing. */

/** File creation flags for open(). */
#define O_CREAT		0x0004		/**< Create file if it does not exist. */
#define O_EXCL		0x0008		/**< Exclusive use flag. */
//#define O_NOCTTY	0x0010		/**< Do not assign controlling terminal. */
#define O_TRUNC		0x0020		/**< Truncate flag. */

/** File status flags for open() and fcntl(). */
#define O_APPEND	0x0040		/**< File offset should be set to end before each write. */
//#define O_NONBLOCK	0x0080		/**< Non-blocking I/O mode. */

/** Other flags for open(). */
#define O_CLOEXEC	0x0100		/**< Open with FD_CLOEXEC flag set. */
#define O_DIRECTORY	0x0200		/**< The call should fail if not a directory. */

/** File descriptor flags for fcntl(). */
#define FD_CLOEXEC	0x0001		/**< File should be closed on execve(). */

/** Action flags for fcntl(). */
#define F_DUPFD		1		/**< Duplicate file descriptor. */
#define F_GETFD		2		/**< Get file descriptor flags. */
#define F_SETFD		3		/**< Set file descriptor flags. */
#define F_GETFL		4		/**< Get file status flags and file access modes. */
#define F_SETFL		5		/**< Set file status flags. */
//#define F_GETLK	6		/**< Get record locking information. */
//#define F_SETLK	7		/**< Set record locking information. */
//#define F_SETLKW	8		/**< Set record locking information; wait if blocked. */
//#define F_GETOWN	9		/**< Get process or process group ID to receive SIGURG signals. */
//#define F_SETOWN	10		/**< Set process or process group ID to receive SIGURG signals. */

extern int open(const char *path, int oflag, ...);
extern int creat(const char *path, mode_t mode);
//extern int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif /* __FCNTL_H */
