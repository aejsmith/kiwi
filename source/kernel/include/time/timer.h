/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Timer management.
 */

#ifndef __TIME_TIMER_H
#define __TIME_TIMER_H

#include <lib/list.h>

struct cpu;

/** Structure containing details of a hardware timer. */
typedef struct timer_device {
	const char *name;		/**< Name of the timer. */
	uint64_t len;			/**< Length of a tick (for periodic timer). */

	/** How the device fires. */
	enum {
		TIMER_DEVICE_PERIODIC,	/**< Timer fires periodically. */
		TIMER_DEVICE_ONESHOT,	/**< Timer fires after the period specified. */
	} type;

	/** Enable the device. */
	void (*enable)(void);

	/** Disable the device (stops it from firing ticks). */
	void (*disable)(void);

	/** Set up the next tick (for one-shot devices).
	 * @param us		Microseconds to fire in. */
	void (*prepare)(timeout_t us);
} timer_device_t;

/** Callback function for timers.
 * @note		This function is called with a spinlock held in
 *			interrupt context! Be careful!
 * @param data		Data argument from timer creator.
 * @return		Whether to reschedule after handling. */
typedef bool (*timer_func_t)(void *data);

/** Structure containing details of a timer. */
typedef struct timer {
	list_t header;			/**< Link to timers list. */

	timeout_t length;		/**< Microseconds until the timer fires. */
	struct cpu *cpu;		/**< CPU that the timer was started on. */
	timer_func_t func;		/**< Function to call when the timer expires. */
	void *data;			/**< Data to pass to timer handler. */
} timer_t;

/** Initialises a statically-declared timer. */
#define TIMER_INITIALISER(_name, _func)	\
	{ \
		.header = LIST_INITIALISER(_name.header), \
		.action = _action, \
		.func   = _func, \
	}

/** Statically declares a timer structure. */
#define TIMER_DECLARE(_name, _action, _func)		\
	timer_t _name = TIMER_INITIALISER(_name, _action, _func)

/** Sleep for a certain number of milliseconds.
 * @param ms		Milliseconds to sleep for. */
#define timer_msleep(s)		timer_usleep((timeout_t)(s) * 1000)

/** Sleep for a certain number of seconds.
 * @param s		Seconds to sleep for. */
#define timer_sleep(s)		timer_usleep((timeout_t)(s) * 1000000)

extern void timer_device_set(timer_device_t *device);
extern bool timer_tick(void);

extern void timer_init(timer_t *timer, timer_func_t func, void *data);
extern void timer_start(timer_t *timer, timeout_t length);
extern void timer_stop(timer_t *timer);

extern void timer_usleep(timeout_t us);

#endif /* __TIME_TIMER_H */
