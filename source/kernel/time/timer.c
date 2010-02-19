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
 *
 * @todo		The way the time until the next tick is set is not
 *			correct with one-shot devices. Using extreme values to
 *			show why, say a timer is set for 50 seconds and it is
 *			the only timer in the list. After 49 seconds, a 49
 *			second timer is set. Because it is placed before the
 *			50 second timer in the list, another 50 seconds will
 *			pass before the 50 second timer fires.
 */

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <time/timer.h>

#include <assert.h>
#include <console.h>
#include <errors.h>

/** Current timer device. */
static timer_device_t *curr_timer_device = NULL;

/** Prepares next timer tick.
 * @param ns		Microseconds until next tick. */
static void timer_tick_prepare(uint64_t us) {
	assert(curr_timer_device);

	/* Only one-shot devices need to be prepared. For periodic devices,
	 * the current tick length is set when the device is enabled. */
	if(curr_timer_device->type != TIMER_DEVICE_ONESHOT) {
		return;
	}

	curr_cpu->tick_len = us;
	curr_timer_device->prepare(us);
}

/** Set the current timer device.
 *
 * Sets the device that will provide timer ticks. The previous device will
 * be disabled.
 *
 * @param device	Device to set.
 */
void timer_device_set(timer_device_t *device) {
	/* Deactivate the old device. */
	if(curr_timer_device) {
		curr_timer_device->disable();
	}

	curr_timer_device = device;

	/* Enable the new source. */
	switch(device->type) {
	case TIMER_DEVICE_PERIODIC:
		curr_cpu->tick_len = device->len;
		device->enable();
		break;
	case TIMER_DEVICE_ONESHOT:
		device->enable();
		device->prepare(curr_cpu->tick_len);
		break;
	default:
		fatal("Invalid timer device type (%d)\n", device->type);
	}

	kprintf(LOG_DEBUG, "timer: activated timer device %p(%s)\n", device, device->name);
}

/** Handles a timer tick.
 *
 * Function called by a timer device when a tick occurs. Goes through all
 * enabled timers for the current CPU and checks if any have expired.
 *
 * @return		Whether to reschedule.
 */
bool timer_tick(void) {
	bool schedule = false;
	timer_t *timer;

	assert(curr_timer_device);

	spinlock_lock(&curr_cpu->timer_lock);

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

		if(timer->func == NULL) {
			fatal("Timer %p has invalid function");
		} else if(timer->func(timer->data)) {
			schedule = true;
		}
	}

	/* Find the next timeout if there is one. */
	if(!list_empty(&curr_cpu->timer_list)) {
		timer = list_entry(curr_cpu->timer_list.next, timer_t, header);
		assert(timer->length > 0);
		timer_tick_prepare(timer->length);
	}

	spinlock_unlock(&curr_cpu->timer_lock);
	return schedule;
}

/** Initialise a timer structure.
 *
 * Initialises a timer structure to contain the given settings.
 *
 * @param timer		Timer to initialise.
 * @param func		Function to call when timer fires.
 * @param data		Data to pass to function.
 */
void timer_init(timer_t *timer, timer_func_t func, void *data) {
	list_init(&timer->header);
	timer->length = 0;
	timer->cpu = NULL;
	timer->func = func;
	timer->data = data;
}

/** Start a timer.
 *
 * Starts a timer to expire after the amount of time specified.
 *
 * @param timer		Timer to start.
 * @param length	Microseconds to run the timer for.
 */
void timer_start(timer_t *timer, useconds_t length) {
	timer_t *exist;

	if(length <= 0) {
		return;
	}

	spinlock_lock(&curr_cpu->timer_lock);

	timer->length = length;
	timer->cpu = curr_cpu;

	/* Place the timer at the end of the list to begin with, and then
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
		timer_tick_prepare(timer->length);
	}

	spinlock_unlock(&curr_cpu->timer_lock);
}

/** Cancel a timer.
 *
 * Cancels a timer that has previously been started with timer_start().
 *
 * @param timer		Timer to stop.
 */
void timer_stop(timer_t *timer) {
	timer_t *next;

	if(!list_empty(&timer->header)) {
		assert(timer->cpu);

		/* Readjust the tick length if required. */
		next = list_entry(timer->cpu->timer_list.next, timer_t, header);
		if(next == timer && timer->header.next != &timer->cpu->timer_list) {
			next = list_entry(timer->header.next, timer_t, header);
			timer_tick_prepare(next->length);
		}

		list_remove(&timer->header);
	}

	timer->length = 0;
	timer->cpu = NULL;
}

/** Sleep for a certain time period.
 *
 * Sends the current thread to sleep for the specified number of microseconds.
 *
 * @param us		Microseconds to sleep for. Must be greater than 0.
 */
void timer_usleep(useconds_t us) {
	waitq_t queue;

	assert(us > 0);

	waitq_init(&queue, "timer_queue");
	waitq_sleep(&queue, us, 0);
}
