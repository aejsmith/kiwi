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
 * @brief		Time handling functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <types.h>

/** Convert seconds to microseconds. */
#define SECS2USECS(secs)	((useconds_t)secs * 1000000)

/** Convert milliseconds to microseconds. */
#define MSECS2USECS(msecs)	((useconds_t)msecs * 1000)

extern useconds_t time_to_unix(int year, int month, int day, int hour, int min, int sec);

extern useconds_t time_from_hardware(void);

extern useconds_t time_since_boot(void);
extern useconds_t time_since_epoch(void);
extern void spin(useconds_t us);

extern void time_init(void);

#endif /* __TIME_H */
