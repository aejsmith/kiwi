/*
 * Copyright (C) 2010 Alex Smith
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

extern status_t kern_timer_create(unsigned flags, handle_t *handlep);
extern status_t kern_timer_start(handle_t handle, useconds_t interval, unsigned mode);
extern status_t kern_timer_stop(handle_t handle, useconds_t *remp);

extern status_t kern_system_time(useconds_t *usp);
extern status_t kern_unix_time(useconds_t *usp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_TIME_H */
