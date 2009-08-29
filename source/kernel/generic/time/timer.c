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

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <time/timer.h>

#include <assert.h>
#include <errors.h>

static clock_source_t *curr_clock = NULL;

/** Prepares next clock tick.
 * @param ns		Nanoseconds until next tick. */
static void clock_prep(uint64_t ns) {
	assert(curr_clock);

	/* Only one-shot sources need to be prepared. For periodic sources,
	 * curr_ticklen is set when the source is enabled. */
	if(curr_clock->type != CLOCK_ONESHOT) {
		return;
	}

	curr_cpu->tick_len = ns;
	curr_clock->prep(ns);
}

/** Set the current clock source.
 *
 * Sets the current clock source to the given source.
 *
 * @param source	Source to set.
 *
 * @return		0 on success, negative error code on failure.
 */
int clock_source_set(clock_source_t *source) {
	/* Deactivate the old source. */
	if(curr_clock != NULL) {
		curr_clock->disable();
	}

	curr_clock = source;

	/* Enable the new source. */
	if(curr_clock->type == CLOCK_PERIODIC) {
		curr_cpu->tick_len = curr_clock->len;
		curr_clock->enable();
	} else if(curr_clock->type == CLOCK_ONESHOT) {
		curr_clock->enable();
		curr_clock->prep(curr_cpu->tick_len);
	}

	kprintf(LOG_DEBUG, "timer: activated clock source %s (source: %p)\n", source->name, source);
	return 0;
}

/** Handles a clock tick.
 *
 * Function called by a clock source when a clock tick occurs. Goes through
 * all enabled timers for the current CPU and checks if any have expired.
 *
 * @return		Whether to reschedule.
 */
bool clock_tick(void) {
	bool resched = false;
	timer_t *timer;

	assert(curr_clock);

	spinlock_lock(&curr_cpu->timer_lock, 0);

	/* Iterate the list and check for expired timers. */
	LIST_FOREACH_SAFE(&curr_cpu->timer_list, iter) {
		timer = list_entry(iter, timer_t, header);

		if(curr_cpu->tick_len < timer->length) {
			timer->length -= curr_cpu->tick_len;
			continue;
		}

		/* Timer has expired, perform its timeout action. */
		list_remove(&timer->header);
		timer->length = 0;
		timer->cpu = NULL;

		switch(timer->action) {
		case TIMER_RESCHEDULE:
			resched = true;
			break;
		case TIMER_FUNCTION:
			if(timer->func == NULL) {
				fatal("Timer %p has invalid function");
			} else if(timer->func()) {
				resched = true;
			}
			break;
		case TIMER_WAKE:
			waitq_wake(&timer->queue, true);
			break;
		default:
			fatal("Bad timer action %d on %p", timer->action, timer);
			break;
		}
	}

	/* Find the next timeout if there is one. */
	if(!list_empty(&curr_cpu->timer_list)) {
		timer = list_entry(curr_cpu->timer_list.next, timer_t, header);
		assert(timer->length > 0);
		clock_prep(timer->length);
	}

	spinlock_unlock(&curr_cpu->timer_lock);
	return resched;
}

/** Initialize a timer structure.
 *
 * Initializes a timer structure to contain the given settings.
 *
 * @param timer		Timer to initialize.
 * @param action	Action to perform when timer expires.
 * @param func		Function to call (for TIMER_FUNCTION).
 */
void timer_init(timer_t *timer, int action, timer_func_t func) {
	list_init(&timer->header);
	waitq_init(&timer->queue, "timer_queue", 0);

	timer->action = action;
	timer->length = 0;
	timer->cpu = NULL;
	timer->func = func;
}

/** Start a timer.
 *
 * Starts a timer to expire after the amount of time specified. If the timer
 * is a TIMER_WAKE timer, then the timer will have expired when the function
 * returns.
 *
 * @param timer		Timer to start.
 * @param length	Nanoseconds to run the timer for.
 *
 * @return		0 on success, negative error code on failure.
 */
int timer_start(timer_t *timer, uint64_t length) {
	timer_t *exist;
	bool state;

	if(length <= 0) {
		kprintf(LOG_DEBUG, "timer: attempted to start timer %p with zero length\n", timer);
		return -ERR_PARAM_INVAL;
	}

	state = intr_disable();
	spinlock_lock_ni(&curr_cpu->timer_lock, 0);

	/* Remove the timer from any list it may be contained in. */
	list_remove(&timer->header);

	timer->length = length;
	timer->cpu = curr_cpu;

	/* Stick the timer at the end of the list to begin with, and then
	 * go through the list to see if we need to move it down before
	 * another one (the list is maintained in shortest to longest order) */
	list_append(&curr_cpu->timer_list, &timer->header);

	LIST_FOREACH_SAFE(&curr_cpu->timer_list, iter) {
		exist = list_entry(iter, timer_t, header);

		if(exist != timer && exist->length > timer->length) {
			list_add_before(&exist->header, &timer->header);
			break;
		}
	}

	/* If the new timer is at the beginning of the list, then it has
	 * the shortest remaining time so we need to adjust the clock
	 * to tick after that amount of time. */
	exist = list_entry(curr_cpu->timer_list.next, timer_t, header);
	if(exist == timer) {
		clock_prep(timer->length);
	}

	spinlock_unlock_ni(&curr_cpu->timer_lock);

	/* If we're a TIMER_WAKE timer, sleep now. */
	if(timer->action == TIMER_WAKE) {
		waitq_sleep(&timer->queue, NULL, 0);
	}

	intr_restore(state);
	return 0;
}

/** Cancel a timer.
 *
 * Cancels a timer that has previously been started with timer_start().
 *
 * @param timer		Timer to stop.
 */
void timer_stop(timer_t *timer) {
	if(!list_empty(&timer->header)) {
		assert(timer->cpu);
		if(list_entry(timer->cpu->timer_list.next, timer_t, header) == timer &&
		   timer->header.next != &timer->cpu->timer_list) {
			clock_prep((list_entry(timer->header.next, timer_t, header))->length);
		}
	}

	list_remove(&timer->header);
	timer->length = 0;
	timer->cpu = NULL;
}

/** Sleep for a certain time period.
 *
 * Sends the current thread to sleep for the specified number of nanoseconds.
 *
 * @param ns		Nanoseconds to sleep for.
 */
void timer_nsleep(uint64_t ns) {
	timer_t timer;

	timer_init(&timer, TIMER_WAKE, NULL);
	timer_start(&timer, ns);
}
