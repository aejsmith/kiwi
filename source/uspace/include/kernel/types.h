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

#ifndef __KERNEL_TYPES_H
#define __KERNEL_TYPES_H

#include <stdint.h>

/** Native-sized types. FIXME: Need architecture definitions of these! */
typedef unsigned long unative_t;	/**< Unsigned native-sized type. */
typedef signed long native_t;		/**< Signed native-sized type. */

/** Kiwi-specific integer types. */
typedef int32_t identifier_t;		/**< Type used to store an identifier for a global object. */
typedef int32_t handle_t;		/**< Type used to store a handle to a per-process object. */
typedef uint32_t timeout_t;		/**< Type used to store a timeout for an operation. */
typedef int64_t offset_t;		/**< Type used to store an offset into something. */
typedef uint64_t key_t;			/**< Type used to identify something. */
typedef uint64_t file_size_t;		/**< Type used to store a file size. */

#endif /* __KERNEL_TYPES_H */
