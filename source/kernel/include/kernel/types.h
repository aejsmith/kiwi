/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Type definitions.
 */

#pragma once

#ifdef __KERNEL
#   include <types.h>
#else
#   define __need_size_t
#   define __need_NULL
#   include <stddef.h>
#   include <stdbool.h>
#   include <stdint.h>
#endif

#ifdef __cplusplus
    #define __KERNEL_EXTERN_C_BEGIN extern "C" {
    #define __KERNEL_EXTERN_C_END   }
#else
    #define __KERNEL_EXTERN_C_BEGIN
    #define __KERNEL_EXTERN_C_END
#endif

#define __kernel_aligned(a)         __attribute__((aligned(a)))
#define __kernel_noreturn           __attribute__((noreturn))

/** Type used to store a kernel status code. */
typedef int32_t status_t;

/** Type used to store a kernel object handle. */
typedef int32_t handle_t;

/** Other integer types used throughout the kernel. */
typedef long ssize_t;               /**< Signed version of size_t. */
typedef int64_t nstime_t;           /**< Type used to store a time value in nanoseconds. */
typedef int64_t offset_t;           /**< Type used to store an offset into an object. */

/** Object identifier types. */
typedef int32_t process_id_t;       /**< Type used to store a process ID. */
typedef int32_t thread_id_t;        /**< Type used to store a thread ID. */
typedef int16_t user_id_t;          /**< Type used to store a user ID. */
typedef int16_t group_id_t;         /**< Type used to store a group ID. */
typedef uint16_t mount_id_t;        /**< Type used to store a mount ID. */
typedef uint64_t node_id_t;         /**< Type used to store a filesystem node ID. */
typedef int16_t image_id_t;         /**< Type used to store a image ID. */
