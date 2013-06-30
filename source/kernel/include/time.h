/*
 * Copyright (C) 2010-2012 Alex Smith
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
 * @brief		Time handling functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <kernel/time.h>
#include <lib/list.h>
#include <types.h>

struct cpu;

/** Convert microseconds to seconds. */
#define USECS2SECS(secs)	(secs / 1000000)

/** Convert seconds to microseconds. */
#define SECS2USECS(secs)	((useconds_t)secs * 1000000)

/** Convert microseconds to milliseconds. */
#define USECS2MSECS(msecs)	(msecs / 1000)

/** Convert milliseconds to microseconds. */
#define MSECS2USECS(msecs)	((useconds_t)msecs * 1000)

/** Structure containing details of a hardware timer. */
typedef struct timer_device {
	const char *name;		/**< Name of the timer. */

	/** Type of the device. */
	enum {
		TIMER_DEVICE_PERIODIC,	/**< Timer fires periodically. */
		TIMER_DEVICE_ONESHOT,	/**< Timer fires after the period specified. */
	} type;

	/** Enable the device (for periodic devices). */
	void (*enable)(void);

	/** Disable the device (for periodic devices). */
	void (*disable)(void);

	/** Set up the next tick (for one-shot devices).
	 * @note		It is OK if the timer fires before the interval
	 * 			is up (for example if the time is outside the
	 * 			range supported by the timer): the timer tick
	 * 			handler will only call handlers if system_time()
	 * 			shows that the timer's target has been reached.
	 * @param us		Microseconds to fire in. */
	void (*prepare)(useconds_t us);
} timer_device_t;

/** Callback function for timers.
 * @warning		Unless TIMER_THREAD is specified in the timer's flags,
 *			this function is called in interrupt context. Be
 *			careful!
 * @param data		Data argument from timer creator.
 * @return		Whether to preempt the current thread after handling.
 *			This is ignored if the function is run in thread
 *			context. */
typedef bool (*timer_func_t)(void *data);

/** Structure containing details of a timer. */
typedef struct timer {
	list_t header;			/**< Link to timers list. */

	useconds_t target;		/**< Time at which the timer will fire. */
	struct cpu *cpu;		/**< CPU that the timer was started on. */
	timer_func_t func;		/**< Function to call when the timer expires. */
	void *data;			/**< Argument to pass to timer handler. */
	unsigned flags;			/**< Behaviour flags. */
	unsigned mode;			/**< Mode of the timer. */
	useconds_t initial;		/**< Initial time (for periodic timers). */
	const char *name;		/**< Name of the timer (for debugging purposes). */
} timer_t;

/** Behaviour flags for timers. */
#define TIMER_THREAD		(1<<0)	/**< Run the handler in thread (DPC) context. */

extern useconds_t time_to_unix(unsigned year, unsigned month, unsigned day,
	unsigned hour, unsigned min, unsigned sec);

extern useconds_t system_time(void);
extern useconds_t unix_time(void);

extern void timer_device_set(timer_device_t *device);
extern bool timer_tick(void);
extern void timer_init(timer_t *timer, const char *name, timer_func_t func,
	void *data, unsigned flags);
extern void timer_start(timer_t *timer, useconds_t length, unsigned mode);
extern void timer_stop(timer_t *timer);

extern status_t delay_etc(useconds_t us, int flags);
extern void delay(useconds_t us);
extern void spin(useconds_t us);

extern useconds_t platform_time_from_hardware(void);

extern void time_init(void);

#endif /* __TIME_H */
