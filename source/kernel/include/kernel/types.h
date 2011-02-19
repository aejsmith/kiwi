/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Type definitions.
 */

#ifndef __KERNEL_TYPES_H
#define __KERNEL_TYPES_H

#ifdef KERNEL
# include <types.h>
#else
# define __need_size_t
# define __need_NULL
# include <stddef.h>
# include <stdbool.h>
# include <stdint.h>
#endif

/** Type used to store a kernel status code. */
typedef int32_t status_t;

/** Type used to store a handle to an object. */
typedef int32_t handle_t;

/** Object identifier types. */
typedef int32_t process_id_t;		/**< Type used to store a process ID. */
typedef int32_t thread_id_t;		/**< Type used to store a thread ID. */
typedef int32_t port_id_t;		/**< Type used to store a port ID. */
typedef int32_t semaphore_id_t;		/**< Type used to store a semaphore ID. */
typedef int32_t area_id_t;		/**< Type used to store a memory area ID. */
typedef int16_t user_id_t;		/**< Type used to store a user ID. */
typedef int16_t group_id_t;		/**< Type used to store a group ID. */
typedef int16_t session_id_t;		/**< Type used to store a session ID. */
typedef uint16_t mount_id_t;		/**< Type used to store a mount ID. */
typedef uint64_t node_id_t;		/**< Type used to store a filesystem node ID. */

/** Other integer types. */
typedef int64_t useconds_t;		/**< Type used to store a time period in microseconds. */
typedef uint64_t offset_t;		/**< Type used to store an offset into something. */
typedef int64_t rel_offset_t;		/**< Type used to store a relative offset. */

#endif /* __KERNEL_TYPES_H */
