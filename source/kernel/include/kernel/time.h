/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Time functions.
 */

#ifndef __KERNEL_TIME_H
#define __KERNEL_TIME_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Timer events. */
#define TIMER_EVENT		0	/**< Event for the timer firing. */

/** Timer flags. */
#define TIMER_SIGNAL		(1<<0)	/**< Send SIGALRM upon timer completion. */

/** Timer mode values. */
#define TIMER_ONESHOT		1	/**< Fire the timer event only once. */
#define TIMER_PERIODIC		2	/**< Fire the event at regular intervals until stopped. */

extern status_t kern_timer_create(int flags, handle_t *handlep);
extern status_t kern_timer_start(handle_t handle, useconds_t interval, int mode);
extern status_t kern_timer_stop(handle_t handle, useconds_t *remp);

extern status_t kern_system_time(useconds_t *usp);
extern status_t kern_unix_time(useconds_t *usp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_TIME_H */
