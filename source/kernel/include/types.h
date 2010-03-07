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

#include <compiler.h>

/** C standard types. */
typedef __SIZE_TYPE__ size_t;		/**< Type to represent the size of an object. */
typedef __PTRDIFF_TYPE__ ptrdiff_t;	/**< Type to store the difference between two pointers. */
typedef _Bool bool;			/**< Boolean type. */

/** Kiwi-specific integer types. */
typedef int32_t process_id_t;		/**< Type used to store a process ID. */
typedef int32_t thread_id_t;		/**< Type used to store a thread ID. */
typedef int32_t port_id_t;		/**< Type used to store a port ID. */
typedef uint16_t mount_id_t;		/**< Type used to store a mount ID. */
typedef uint64_t node_id_t;		/**< Type used to store a filesystem node ID. */
typedef int32_t handle_t;		/**< Type used to store a handle to an object. */
typedef int64_t useconds_t;		/**< Type used to store a time period in microseconds. */
typedef int64_t offset_t;		/**< Type used to store an offset into something. */
typedef uint64_t file_size_t;		/**< Type used to store a file size. */

/** TODO: To be removed. */
typedef int32_t identifier_t;		/**< Type used to store an identifier for a global object. */

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
#define INT8_MIN	(-0x80)
#define INT8_MAX	0x7F
#define UINT8_MAX	0xFFu
#define INT16_MIN	(-0x8000)
#define INT16_MAX	0x7FFF
#define UINT16_MAX	0xFFFFu
#define INT32_MIN	(-0x80000000)
#define INT32_MAX	0x7FFFFFFF
#define UINT32_MAX	0xFFFFFFFFu
#define INT64_MIN	(-0x8000000000000000ll)
#define INT64_MAX	0x7FFFFFFFFFFFFFFFll
#define UINT64_MAX	0xFFFFFFFFFFFFFFFFull

#endif /* __TYPES_H */
