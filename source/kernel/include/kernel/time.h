/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Time functions.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

/** Timer object events. */
enum {
    /** Event for the timer firing. */
    TIMER_EVENT     = 1,
};

/** Timer mode values. */
enum {
    TIMER_ONESHOT   = 1,            /**< Fire the timer event only once. */
    TIMER_PERIODIC  = 2,            /**< Fire the event at regular intervals until stopped. */
};

extern status_t kern_timer_create(uint32_t flags, handle_t *_handle);
extern status_t kern_timer_start(handle_t handle, nstime_t interval, uint32_t mode);
extern status_t kern_timer_stop(handle_t handle, nstime_t *_rem);

/** Time sources. */
enum {
    TIME_SYSTEM     = 1,            /**< Monotonic system time. */
    TIME_REAL       = 2,            /**< Real time (time since UNIX epoch). */
};

extern status_t kern_time_get(uint32_t source, nstime_t *_time);
extern status_t kern_time_set(uint32_t source, nstime_t time);

__KERNEL_EXTERN_C_END
