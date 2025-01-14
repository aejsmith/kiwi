/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               I/O context functions.
 */

#include <io/fs.h>

#include <proc/process.h>

#include <assert.h>
#include <status.h>

/**
 * Initializes a new process' I/O context. If the process has a parent, the
 * new context will inherit the parent's root and current directories. If no
 * parent is specified, the current and root directories will be set to the
 * absolute root of the filesystem.
 *
 * @param process       Process to initialize.
 * @param parent        Parent process (can be NULL).
 */
void io_process_init(process_t *process, process_t *parent) {
    rwlock_init(&process->io.lock, "io_context_lock");

    /* Inherit parent's current/root directories if possible. */
    if (parent) {
        rwlock_read_lock(&parent->io.lock);

        assert(parent->io.root_dir);
        assert(parent->io.curr_dir);

        fs_dentry_retain(parent->io.root_dir);
        process->io.root_dir = parent->io.root_dir;
        fs_dentry_retain(parent->io.curr_dir);
        process->io.curr_dir = parent->io.curr_dir;

        rwlock_unlock(&parent->io.lock);
    } else if (root_mount) {
        fs_dentry_retain(root_mount->root);
        process->io.root_dir = root_mount->root;
        fs_dentry_retain(root_mount->root);
        process->io.curr_dir = root_mount->root;
    } else {
        /* This should only be the case when the kernel process is being
         * created. They will be set when the FS is initialized. */
        assert(!kernel_proc);

        process->io.curr_dir = NULL;
        process->io.root_dir = NULL;
    }
}

/** Destroy an process' I/O context.
 * @param process       Process being destroyed. */
void io_process_cleanup(process_t *process) {
    fs_dentry_release(process->io.curr_dir);
    fs_dentry_release(process->io.root_dir);
}
