/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Process group API.
 *
 * The process group API allows for tracking of arbitrary groups of processes.
 * A process group is an object that can have processes added to, and any
 * child processes that a process in a group creates can be automatically be
 * added to the group as well. The owner of a process group can query whether a
 * process is a member of a group.
 *
 * There are no limits on the number of groups a process can belong to, and all
 * groups are independent from each other.
 */

#pragma once

#include <kernel/object.h>

__KERNEL_EXTERN_C_BEGIN

/** Process group flags. */
enum {
    /** New children of processes in the group get automatically added to it. */
    PROCESS_GROUP_INHERIT_MEMBERSHIP    = (1<<0),
};

/** Process group object events. */
enum {
    /**
     * Wait for process group death, i.e. there are no longer any running
     * processes in the group.
     */
    PROCESS_GROUP_EVENT_DEATH   = 1,
};

extern status_t kern_process_group_create(uint32_t flags, handle_t *_handle);
extern status_t kern_process_group_add(handle_t handle, handle_t process);
extern status_t kern_process_group_remove(handle_t handle, handle_t process);
extern status_t kern_process_group_query(handle_t handle, handle_t process);
extern status_t kern_process_group_enumerate(handle_t handle, process_id_t *_ids, size_t *_count);

__KERNEL_EXTERN_C_END
