/* Kiwi timer management
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

#include <cpu/intr.h>

#include <sync/waitq.h>

#include <types.h>

struct cpu;

/** Structure containing details of a clock source. */
typedef struct clock_source {
	const char *name;		/**< Name of the clock source. */
	uint64_t len;			/**< Length of a tick (for periodic sources). */

	/** Type of the source. */
	enum {
		CLOCK_PERIODIC,		/**< Clock ticks periodically. */
		CLOCK_ONESHOT,		/**< Clock is configured to tick once after a certain time. */
	} type;

	/** Clock operations. */
	void (*prep)(uint64_t ns);	/**< Prepares the next tick (for one-shot sources). */
	void (*enable)(void);		/**< Enables the clock. */
	void (*disable)(void);		/**< Disables the source (stops ticks from being received). */
} clock_source_t;

/** Function type for TIMER_FUNCTION timers.
 * @return		Whether to reschedule after handling. */
typedef bool (*timer_func_t)(void);

/** Structure containing details of a timer. */
typedef struct timer {
	list_t header;			/**< Link to timers list. */

	/** Action to perform when timer expires. */
	enum {
		TIMER_RESCHEDULE,	/**< Perform a thread switch. */
		TIMER_FUNCTION,		/**< Call the function specified in the timer. */
		TIMER_WAKE,		/**< Wake the thread that started the timer. */
	} action;

	uint64_t length;		/**< Nanoseconds until the timer expires. */
	struct cpu *cpu;		/**< CPU that the timer was started on. */
	timer_func_t func;		/**< Function to call upon expiry. */
	wait_queue_t queue;		/**< Wait queue for TIMER_WAKE timers. */
} timer_t;

/** Initializes a statically-declared timer. */
#define TIMER_INITIALIZER(_name, _action, _func)	\
	{ \
		.header = LIST_INITIALIZER(_name.header), \
		.action = _action, \
		.func   = _func, \
	}

/** Statically declares a timer structure. */
#define TIMER_DECLARE(_name, _action, _func)		\
	timer_t _name = TIMER_INITIALIZER(_name, _action, _func)

/** Sleep for a certain number of microseconds.
 * @param us		Microseconds to sleep for. */
#define timer_usleep(us)	timer_nsleep((uint64_t)(us) * 1000)

/** Sleep for a certain number of seconds.
 * @param s		Seconds to sleep for. */
#define timer_sleep(s)		timer_nsleep((uint64_t)(s) * 1000000000)

extern int clock_source_set(clock_source_t *source);
extern intr_result_t clock_tick(void);

extern void timer_init(timer_t *timer, int action, timer_func_t func);
extern int timer_start(timer_t *timer, uint64_t length);
extern void timer_stop(timer_t *timer);

extern void timer_nsleep(uint64_t ns);

#endif /* __TIME_TIMER_H */
