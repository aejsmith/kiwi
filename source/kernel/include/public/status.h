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
#  define STATUS_ALREADY_EXISTS		7	/**< Object already exists. */
#  define STATUS_NOT_FOUND		8	/**< Requested object not found. */
#  define STATUS_TYPE_INVAL		9	/**< Object type is invalid. */
#define STATUS_READ_ONLY		10	/**< Object cannot be modified. */
#define STATUS_ADDR_INVAL		11	/**< A bad memory location was specified. */
#define STATUS_RESOURCE_UNAVAIL		12	/**< Resource is temporarily unavailable. */
#define STATUS_IN_USE			13	/**< Object is in use. */
#  define STATUS_NO_SPACE		14	/**< No space is available. */
#define STATUS_PERM_DENIED		15	/**< Permission denied. */
#define STATUS_TOO_LONG			16	/**< Provided string is too long. */
#define STATUS_LINK_LIMIT		17	/**< Exceeded nested symbolic link limit. */
#  define STATUS_BUF_TOO_SMALL		18	/**< Provided buffer is too small. */
#define STATUS_SYSCALL_INVAL		19	/**< Invalid system call number. */
#define STATUS_DEST_UNREACHABLE		20	/**< Cannot reach destination. */
#define STATUS_TIMED_OUT		21	/**< Timed out while waiting. */
#  define STATUS_DEVICE_ERROR		22	/**< There was an error on the device. */
#define STATUS_OVERFLOW			23	/**< Integer overflow. */
#  define STATUS_NOT_EMPTY		24	/**< Directory is not empty. */
#define STATUS_PROCESS_RUNNING		25	/**< Process is still running. */
#define STATUS_UNKNOWN_IMAGE		26	/**< Executable image has an unrecognised format. */
#define STATUS_MALFORMED_IMAGE		27	/**< Executable image format is incorrect. */
#define STATUS_MISSING_LIBRARY		28	/**< Required library not found. */
#define STATUS_MISSING_SYMBOL		29	/**< Referenced symbol not found. */
#define STATUS_UNKNOWN_FS		30	/**< Filesystem has an unrecognised format. */

#endif /* __KERNEL_STATUS_H */
