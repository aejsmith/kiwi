/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Condition object.
 */

#pragma once

#include <kernel/object.h>

__KERNEL_EXTERN_C_BEGIN

/** Events that can occur on condition objects. */
enum {
    /** Condition has been set to true. */
    CONDITION_EVENT_SET = 1,
};

extern status_t kern_condition_set(handle_t handle, bool state);
extern status_t kern_condition_create(handle_t *_handle);

__KERNEL_EXTERN_C_END
