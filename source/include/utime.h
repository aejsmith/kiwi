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
 * @brief		Access/modification time functions.
 */

#ifndef __UTIME_H
#define __UTIME_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** POSIX utimbuf structure. */
struct utimbuf {
	time_t actime;			/**< Access time. */
	time_t modtime;			/**< Modification time. */
};

extern int utime(const char *path, const struct utimbuf *times);

#ifdef __cplusplus
}
#endif

#endif /* __UTIME_H */
