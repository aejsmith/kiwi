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

// TODO: indented lines are to be replaced by more specific error codes or
// changed.

// TODO: Rename _INVAL codes to _INVALID_BLAH

/** Definitions of status codes returned by kernel functions. */
#define STATUS_SUCCESS			0	/**< Operation completed successfully. */
#define STATUS_NO_MEMORY		1	/**< No memory available. */
#define STATUS_PARAM_INVAL		2	/**< Invalid parameter specified. */
#define STATUS_WOULD_BLOCK		3	/**< Operation would block. */
#define STATUS_INTERRUPTED		4	/**< Interrupted while blocking. */
#define STATUS_NOT_IMPLEMENTED		5	/**< Operation not implemented. */
#define STATUS_NOT_SUPPORTED		6	/**< Operation not supported. */
#define STATUS_DEP_MISSING		7	/**< Required dependency not found. */
#  define STATUS_FORMAT_INVAL		8	/**< Object's format is incorrect. */
#  define STATUS_ALREADY_EXISTS		9	/**< Object already exists. */
#  define STATUS_NOT_FOUND		10	/**< Requested object not found. */
#  define STATUS_TYPE_INVAL		11	/**< Object type is invalid. */
#define STATUS_READ_ONLY		12	/**< Object cannot be modified. */
#define STATUS_ADDR_INVAL		13	/**< A bad memory location was specified. */
#define STATUS_RESOURCE_UNAVAIL		14	/**< Resource is temporarily unavailable. */
#define STATUS_IN_USE			15	/**< Object is in use. */
#  define STATUS_NO_SPACE		16	/**< No space is available. */
#define STATUS_PERM_DENIED		17	/**< Permission denied. */
#define STATUS_TOO_LONG			18	/**< Provided string is too long. */
#define STATUS_LINK_LIMIT		19	/**< Exceeded nested symbolic link limit. */
#  define STATUS_BUF_TOO_SMALL		20	/**< Provided buffer is too small. */
#define STATUS_SYSCALL_INVAL		21	/**< Invalid system call number. */
#define STATUS_DEST_UNREACHABLE		22	/**< Cannot reach destination. */
#define STATUS_TIMED_OUT		23	/**< Timed out while waiting. */
#  define STATUS_DEVICE_ERROR		24	/**< There was an error on the device. */
#define STATUS_OVERFLOW			25	/**< Integer overflow. */
#  define STATUS_NOT_EMPTY		26	/**< Directory is not empty. */
#define STATUS_PROCESS_RUNNING		27	/**< Process is still running. */

#endif /* __KERNEL_STATUS_H */
