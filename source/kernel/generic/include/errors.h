/* Kiwi error number definitions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Error number definitions.
 */

#ifndef __ERRORS_H
#define __ERRORS_H

/** Definitions of error codes returned by kernel functions. */
#define ERR_NO_MEMORY		1	/**< No memory available. */
#define ERR_PARAM_INVAL		2	/**< Invalid parameter specified. */
#define ERR_WOULD_BLOCK		3	/**< Operation would block. */
#define ERR_INTERRUPTED		4	/**< Interrupted while blocking. */
#define ERR_NOT_IMPLEMENTED	5	/**< Operation not implemented. */
#define ERR_NOT_SUPPORTED	6	/**< Operation not supported. */
#define ERR_DEP_MISSING		7	/**< Required dependency not found. */
#define ERR_FORMAT_INVAL	8	/**< Object's format is incorrect. */
#define ERR_ALREADY_EXISTS	9	/**< Object already exists. */
#define ERR_NOT_FOUND		10	/**< Requested object not found. */
#define ERR_TYPE_INVAL		11	/**< Object type is invalid. */
#define ERR_READ_ONLY		12	/**< Object cannot be modified. */
#define ERR_ADDR_INVAL		13	/**< A bad memory location was specified. */
#define ERR_NO_HANDLES		14	/**< Handle table is full. */
#define ERR_IN_USE		15	/**< Object is in use. */
#define ERR_NO_SPACE		16	/**< No space is available. */
#define ERR_PERM_DENIED		17	/**< Permission denied. */
#define ERR_STR_TOO_LONG	18	/**< String is too long. */

#endif /* __ERRORS_H */
