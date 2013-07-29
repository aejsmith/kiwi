/*
 * Copyright (C) 2010-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		POSIX alarm function.
 */

#include <kernel/mutex.h>
#include <kernel/time.h>
#include <kernel/status.h>

#include <unistd.h>

#include "libsystem.h"

/** Alarm timer handle. */
static handle_t alarm_handle = -1;
static int32_t alarm_lock = MUTEX_INITIALIZER;

/** Arrange for a SIGALRM signal to be delivered after a certain time.
 * @param seconds	Seconds to wait for.
 * @return		Seconds until previously scheduled alarm was to be
 *			delivered, or 0 if no previous alarm. */
unsigned int alarm(unsigned int seconds) {
	useconds_t rem;
	status_t ret;

	kern_mutex_lock(&alarm_lock, -1);

	/* Create the alarm timer if it has not already been created. */
	if(alarm_handle < 0) {
		ret = kern_timer_create(TIMER_SIGNAL, &alarm_handle);
		if(ret != STATUS_SUCCESS) {
			/* Augh, POSIX doesn't let this fail. */
			libsystem_fatal("failed to create alarm timer (%d)", ret);
		}
	}

	kern_timer_stop(alarm_handle, &rem);
	kern_timer_start(alarm_handle, seconds * 1000000000, TIMER_ONESHOT);
	kern_mutex_unlock(&alarm_lock);
	return rem / 1000000000;
}
