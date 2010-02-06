/* Kiwi PC Programmable Interval Timer code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		PC Programmable Interval Timer code.
 */

#ifndef __PLATFORM_PIT_H
#define __PLATFORM_PIT_H

#include <time/timer.h>

/** Frequency to use for PIT (in Hz). */
#define PIT_FREQUENCY		1000

extern timer_device_t pit_timer_device;

#endif /* __PLATFORM_PIT_H */