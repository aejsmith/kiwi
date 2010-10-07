/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Kernel status code definitions.
 */

#ifndef __KERNEL_STATUS_H
#define __KERNEL_STATUS_H

/** Definitions of status codes returned by kernel functions. */
#define STATUS_SUCCESS			0	/**< Operation completed successfully. */
#define STATUS_NOT_IMPLEMENTED		1	/**< Operation not implemented. */
#define STATUS_NOT_SUPPORTED		2	/**< Operation not supported. */
#define STATUS_WOULD_BLOCK		3	/**< Operation would block. */
#define STATUS_INTERRUPTED		4	/**< Interrupted while blocking. */
#define STATUS_TIMED_OUT		5	/**< Timed out while waiting. */
#define STATUS_INVALID_SYSCALL		6	/**< Invalid system call number. */
#define STATUS_INVALID_ARG		7	/**< Invalid argument specified. */
#define STATUS_INVALID_HANDLE		8	/**< Non-existant handle or handle with incorrect type. */
#define STATUS_INVALID_ADDR		9	/**< Invalid memory location specified. */
#define STATUS_INVALID_REQUEST		10	/**< Invalid device request specified. */
#define STATUS_INVALID_EVENT		11	/**< Invalid object event specified. */
#define STATUS_OVERFLOW			12	/**< Integer overflow. */
#define STATUS_NO_MEMORY		13	/**< Out of memory. */
#define STATUS_NO_HANDLES		14	/**< No handles are available. */
#define STATUS_NO_PORTS			15	/**< No ports are available. */
#define STATUS_NO_SEMAPHORES		16	/**< No semaphores are available. */
#define STATUS_NO_AREAS			17	/**< No shared memory areas are available. */
#define STATUS_READ_ONLY		18	/**< Object cannot be modified. */
#define STATUS_PERM_DENIED		19	/**< Permission denied. */
#define STATUS_NOT_DIR			20	/**< Path component is not a directory. */
#define STATUS_NOT_FILE			21	/**< Path does not refer to a file. */
#define STATUS_NOT_SYMLINK		22	/**< Path does not refer to a symbolic link. */
#define STATUS_NOT_MOUNT		23	/**< Path does not refer to root of a mount. */
#define STATUS_NOT_FOUND		24	/**< Requested object could not be found. */
#define STATUS_ALREADY_EXISTS		25	/**< Object already exists. */
#define STATUS_TOO_SMALL		26	/**< Provided buffer is too small. */
#define STATUS_TOO_LONG			27	/**< Provided string is too long. */
#define STATUS_DIR_NOT_EMPTY		28	/**< Directory is not empty. */
#define STATUS_DIR_FULL			29	/**< Directory is full. */
#define STATUS_UNKNOWN_FS		30	/**< Filesystem has an unrecognised format. */
#define STATUS_CORRUPT_FS		31	/**< Corruption detected on the filesystem. */
#define STATUS_FS_FULL			32	/**< No space is available on the filesystem. */
#define STATUS_SYMLINK_LIMIT		33	/**< Exceeded nested symbolic link limit. */
#define STATUS_IN_USE			34	/**< Object is in use. */
#define STATUS_DEVICE_ERROR		35	/**< An error occurred during a hardware operation. */
#define STATUS_PROCESS_RUNNING		36	/**< Process is still running. */
#define STATUS_UNKNOWN_IMAGE		37	/**< Executable image has an unrecognised format. */
#define STATUS_MALFORMED_IMAGE		38	/**< Executable image format is incorrect. */
#define STATUS_MISSING_LIBRARY		39	/**< Required library not found. */
#define STATUS_MISSING_SYMBOL		40	/**< Referenced symbol not found. */
#define STATUS_DEST_UNREACHABLE		41	/**< Cannot reach destination. */
#define STATUS_TRY_AGAIN		42	/**< Attempt the operation again. */

#if !defined(KERNEL) && !defined(__ASM__)
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *__kernel_status_strings[];
extern size_t __kernel_status_size;

#ifdef __cplusplus
}
#endif
#endif
#endif /* __KERNEL_STATUS_H */
