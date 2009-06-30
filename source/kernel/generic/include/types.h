/* Kiwi type definitions
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
 * @brief		Type definitions.
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <arch/types.h>

#include <compiler.h>

/** C standard types. */
typedef __SIZE_TYPE__ size_t;		/**< Type to represent the size of an object. */
typedef __PTRDIFF_TYPE__ ptrdiff_t;	/**< Type to store the difference between two pointers. */
typedef _Bool bool;			/**< Boolean type. */

/** Kiwi-specific types. */
typedef uint32_t identifier_t;		/**< Type used to store an object identifier. */
typedef int64_t offset_t;		/**< Type used to store an offset into something. */
typedef uint64_t key_t;			/**< Type used to identify something. */

/** Format definitions for Kiwi-specific types. */
#define PRIo		PRId64		/**< Format character for offset_t. */

/** Various definitions. */
#define false		0		/**< False boolean value. */
#define true		1		/**< True boolean value. */
#define NULL		0		/**< NULL value for a pointer. */

/** Gets the offset of a member in a type. */
#define offsetof(type, member)		\
	__builtin_offsetof(type, member)

#endif /* __TYPES_H */
