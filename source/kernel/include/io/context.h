/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               I/O context functions.
 */

#pragma once

#include <sync/rwlock.h>

struct fs_dentry;
struct process;

/** Structure containing an I/O context. */
typedef struct io_context {
    rwlock_t lock;                  /**< Lock to protect context. */
    struct fs_dentry *root_dir;     /**< Root directory. */
    struct fs_dentry *curr_dir;     /**< Current working directory. */
} io_context_t;

extern void io_process_init(struct process *process, struct process *parent);
extern void io_process_cleanup(struct process *process);
