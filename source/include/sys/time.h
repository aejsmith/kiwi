/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		POSIX time functions/definitions.
 */

#ifndef __SYS_TIME_H
#define __SYS_TIME_H

//#include <sys/select.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Time value structure. */
struct timeval {
	time_t tv_sec;			/**< Seconds. */
	suseconds_t tv_usec;		/**< Additional microseconds since. */
};

//extern int gettimeofday(struct timeval *tv, void *tz);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TIME_H */
