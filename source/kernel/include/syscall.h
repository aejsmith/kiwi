/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               System call dispatcher.
 */

#pragma once

#include <types.h>

/** Structure describing a system call handler. */
typedef struct __packed syscall {
    ptr_t addr;                 /**< Address of handler. */
    size_t count;               /**< Number of arguments. */
} syscall_t;
