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
