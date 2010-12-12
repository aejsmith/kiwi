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
 * @brief		POSIX time function.
 */

#include <kernel/time.h>
#include <sys/time.h>
#include <errno.h>

/** Get the current time.
 * @param tv		Structure to fill with time since epoch.
 * @param tz		Pointer to timezone (ignored).
 * @return		0 on success, -1 on failure. */
int gettimeofday(struct timeval *tv, void *tz) {
	useconds_t ktime;

	kern_unix_time(&ktime);
	tv->tv_sec = ktime / 1000000;
	tv->tv_usec = ktime % 1000000;
	return 0;
}
