/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Semaphore object.
 */

#pragma once

#include <kernel/object.h>

__KERNEL_EXTERN_C_BEGIN

/** Event for the semaphore count becoming non-zero. */
#define SEMAPHORE_EVENT     1

extern status_t kern_semaphore_create(size_t count, handle_t *_handle);
extern status_t kern_semaphore_down(handle_t handle, nstime_t timeout);
extern status_t kern_semaphore_up(handle_t handle, size_t count);

__KERNEL_EXTERN_C_END
