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

#include <lib/list.h>
#include <types.h>

/** Convert microseconds to seconds. */
#define USECS2SECS(secs)	(secs / 1000000)

/** Convert seconds to microseconds. */
#define SECS2USECS(secs)	((useconds_t)secs * 1000000)

/** Convert microseconds to milliseconds. */
#define USECS2MSECS(msecs)	(msecs / 1000)

/** Convert milliseconds to microseconds. */
#define MSECS2USECS(msecs)	((useconds_t)msecs * 1000)

#ifndef LOADER

#include <kernel/time.h>

struct cpu;

/** Structure containing details of a hardware timer. */
typedef struct timer_device {
	const char *name;		/**< Name of the timer. */
	useconds_t len;			/**< Length of a tick (for periodic timer). */

	/** Type of the device. */
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
	void (*prepare)(useconds_t us);
} timer_device_t;

/** Callback function for timers.
 * @note		Unless TIMER_THREAD is specified in the timer's flags,
 *			this function is called with a spinlock held in
 *			interrupt context. Be careful!
 * @param data		Data argument from timer creator.
 * @return		Whether to reschedule after handling. This is ignored
 *			if the function is run in thread context. */
typedef bool (*timer_func_t)(void *data);

/** Structure containing details of a timer. */
typedef struct timer {
	list_t header;			/**< Link to timers list. */
	useconds_t target;		/**< Time at which the timer will fire. */
	struct cpu *cpu;		/**< CPU that the timer was started on. */
	timer_func_t func;		/**< Function to call when the timer expires. */
	void *data;			/**< Argument to pass to timer handler. */
	int flags;			/**< Behaviour flags. */
	int mode;			/**< Current mode of the timer. */
	useconds_t initial;		/**< Initial time (if periodic). */
} timer_t;

/** Behaviour flags for timers. */
#define TIMER_THREAD		(1<<0)	/**< Run the handler in thread (DPC) context. */

extern useconds_t time_to_unix(int year, int month, int day, int hour, int min, int sec);
extern useconds_t time_from_hardware(void);
extern useconds_t time_since_boot(void);
extern useconds_t time_since_epoch(void);

extern void timer_device_set(timer_device_t *device);
extern bool timer_tick(void);
extern void timer_init(timer_t *timer, timer_func_t func, void *data, int flags);
extern void timer_start(timer_t *timer, useconds_t length, int mode);
extern void timer_stop(timer_t *timer);

extern status_t usleep_etc(useconds_t us, int flags);
extern void usleep(useconds_t us);

extern int kdbg_cmd_timers(int argc, char **argv);
extern int kdbg_cmd_uptime(int argc, char **argv);

extern void time_arch_init(void);
extern void time_init(void);

#endif /* LOADER */

extern void spin(useconds_t us);

#endif /* __TIME_H */
