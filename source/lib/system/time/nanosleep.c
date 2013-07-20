/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX nanosecond sleep function.
 *
 * @todo		This is currently only microsecond resolution.
 */

#include <kernel/status.h>
#include <kernel/thread.h>

#include <errno.h>
#include <time.h>

/** High resolution sleep.
 * @param rqtp		Requested sleep time.
 * @param rmtp		Where to store remaining time if interrupted.
 * @return		0 on success, -1 on failure. */
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
	useconds_t rem;
	status_t ret;
	uint64_t ns;

	if(rqtp->tv_sec < 0 || rqtp->tv_nsec < 0 || rqtp->tv_nsec >= 1000000000) {
		errno = EINVAL;
		return -1;
	}

	ns = ((uint64_t)rqtp->tv_sec * 1000000000) + rqtp->tv_nsec;
	ret = kern_thread_usleep(ns / 1000, &rem);
	if(ret == STATUS_INTERRUPTED) {
		if(rmtp) {
			ns = rem * 1000;
			rmtp->tv_nsec = ns % 1000000000;
			rmtp->tv_sec = ns / 1000000000;
		}
		errno = EINTR;
		return -1;
	}

	return 0;
}
