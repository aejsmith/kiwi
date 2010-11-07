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
 * @brief		POSIX sleep function.
 */

#include <errno.h>
#include <time.h>
#include <unistd.h>

/** Sleep for a certain interval.
 * @param secs		Number of seconds to sleep for.
 * @return		0, or number of seconds remaining if interrupted. */
unsigned int sleep(unsigned int secs) {
	struct timespec ts;

	ts.tv_sec = secs;
	ts.tv_nsec = 0;
	if(nanosleep(&ts, &ts) == -1 && errno == EINTR) {
		return ts.tv_sec;
	}
	return 0;
}
