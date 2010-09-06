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
 * @brief		Type definitions.
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <arch/types.h>
#include <public/types.h>
#include <compiler.h>

/** C standard types. */
typedef __SIZE_TYPE__ size_t;		/**< Type to represent the size of an object. */
typedef __PTRDIFF_TYPE__ ptrdiff_t;	/**< Type to store the difference between two pointers. */
typedef _Bool bool;			/**< Boolean type. */

/** Internal kernel integer types. */
typedef uint64_t key_t;			/**< Type used to store a key for a container. */

/** Various definitions. */
#define false		0		/**< False boolean value. */
#define true		1		/**< True boolean value. */
#define NULL		0		/**< NULL value for a pointer. */

/** Gets the offset of a member in a type. */
#define offsetof(type, member)		\
	__builtin_offsetof(type, member)

/** Type limit macros. */
#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255u
#define INT16_MIN	(-32767-1)
#define INT16_MAX	32767
#define UINT16_MAX	65535u
#define INT32_MIN	(-2147483647-1)
#define INT32_MAX	2147483647
#define UINT32_MAX	4294967295u
#define INT64_MIN	(-9223372036854775807ll-1)
#define INT64_MAX	9223372036854775807ll
#define UINT64_MAX	18446744073709551615ull

#endif /* __TYPES_H */
