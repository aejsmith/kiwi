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
 *
 * @todo		For the moment, this code assumes that the hardware
 *			RTC is storing the time as UTC.
 */

#include <cpu/cpu.h>

#include <sync/waitq.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <kdbg.h>
#include <time.h>

/** Check if a year is a leap year. */
#define LEAPYR(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/** Get number of days in a year. */
#define DAYS(y)		(LEAPYR(y) ? 366 : 365)

/** Table containing number of days before a month. */
static int days_before_month[] = {
	0,
	/* Jan. */ 0,
	/* Feb. */ 31,
	/* Mar. */ 31 + 28,
	/* Apr. */ 31 + 28 + 31,
	/* May. */ 31 + 28 + 31 + 30,
	/* Jun. */ 31 + 28 + 31 + 30 + 31,
	/* Jul. */ 31 + 28 + 31 + 30 + 31 + 30,
	/* Aug. */ 31 + 28 + 31 + 30 + 31 + 30 + 31,
	/* Sep. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
	/* Oct. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
	/* Nov. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
	/* Dec. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
};

/** The number of microseconds since the Epoch the kernel was booted at. */
static useconds_t boot_unix_time = 0;

/** Current timer device. */
static timer_device_t *curr_timer_device = NULL;

/** Prepares next timer tick.
 * @param timer		Timer to prepare for. */
static void timer_tick_prepare(timer_t *timer) {
	useconds_t length;

	assert(curr_timer_device);

	/* Only one-shot devices need to be prepared. For periodic devices,
	 * the current tick length is set when the device is enabled. */
	if(curr_timer_device->type == TIMER_DEVICE_ONESHOT) {
		length = timer->target - time_since_boot();
		curr_timer_device->prepare((length > 0) ? length : 1);
	}
}

/** Convert a date/time to microseconds since the epoch.
 * @param year		Year.
 * @param month		Month (1-12).
 * @param day		Day of month (1-31).
 * @param hour		Hour (0-23).
 * @param min		Minute (0-59).
 * @param sec		Second (0-59).
 * @return		Number of microseconds since the epoch. */
useconds_t time_to_unix(int year, int month, int day, int hour, int min, int sec) {
	uint32_t seconds = 0;
	int i;

	/* Start by adding the time of day and day of month together. */
	seconds += sec;
	seconds += min * 60;
	seconds += hour * 60 * 60;
	seconds += (day - 1) * 24 * 60 * 60;

	/* Convert the month into days. */
	seconds += days_before_month[month] * 24 * 60 * 60;

	/* If this year is a leap year, and we're past February, we need to
	 * add another day. */
	if(month > 2 && LEAPYR(year)) {
		seconds += 24 * 60 * 60;
	}

	/* Add the days in each year before this year from 1970. */
	for(i = 1970; i < year; i++) {
		seconds += DAYS(i) * 24 * 60 * 60;
	}

	return SECS2USECS(seconds);
}

/** Get the number of microseconds since the Unix Epoch.
 *
 * Returns the number of microseconds that have passed since the Unix epoch,
 * 00:00:00 UTC, January 1st, 1970.
 *
 * @return		Number of microseconds since epoch.
 */
useconds_t time_since_epoch(void) {
	return boot_unix_time + time_since_boot();
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
	device->enable();
	kprintf(LOG_NORMAL, "timer: activated timer device %s\n", device->name);
}

/** Handles a timer tick.
 * @return		Whether to reschedule. */
bool timer_tick(void) {
	useconds_t time = time_since_boot();
	bool schedule = false;
	timer_t *timer;

	assert(curr_timer_device);

	spinlock_lock(&curr_cpu->timer_lock);

	/* Iterate the list and check for expired timers. */
	LIST_FOREACH_SAFE(&curr_cpu->timers, iter) {
		timer = list_entry(iter, timer_t, header);

		/* Since the list is ordered soonest expiry to furthest expiry
		 * first, we can break if the current timer has not expired. */
		if(time < timer->target) {
			break;
		}

		/* Timer has expired, perform its timeout action. */
		list_remove(&timer->header);
		if(timer->func(timer->data)) {
			schedule = true;
		}
	}

	/* Find the next timeout if there is one. */
	if(!list_empty(&curr_cpu->timers)) {
		timer_tick_prepare(list_entry(curr_cpu->timers.next, timer_t, header));
	}

	spinlock_unlock(&curr_cpu->timer_lock);
	return schedule;
}

/** Initialise a timer structure.
 * @param timer		Timer to initialise.
 * @param func		Function to call when the timer expires.
 * @param data		Data argument to pass to timer. */
void timer_init(timer_t *timer, timer_func_t func, void *data) {
	list_init(&timer->header);
	timer->func = func;
	timer->data = data;
}

/** Start a timer.
 * @param timer		Timer to start.
 * @param length	Microseconds to run the timer for. If 0 or negative
 *			the function will do nothing. */
void timer_start(timer_t *timer, useconds_t length) {
	timer_t *exist;

	if(length <= 0) {
		return;
	}

	timer->target = time_since_boot() + length;
	timer->cpu = curr_cpu;

	spinlock_lock(&curr_cpu->timer_lock);

	/* Place the timer at the end of the list to begin with, and then
	 * go through the list to see if we need to move it down before
	 * another one (the list is ordered with nearest first). */
	list_append(&curr_cpu->timers, &timer->header);
	LIST_FOREACH_SAFE(&curr_cpu->timers, iter) {
		exist = list_entry(iter, timer_t, header);
		if(exist != timer && exist->target > timer->target) {
			list_add_before(&exist->header, &timer->header);
			break;
		}
	}

	/* If the new timer is at the beginning of the list, then it has
	 * the shortest remaining time so we need to adjust the clock
	 * to tick after that amount of time. */
	if(curr_cpu->timers.next == &timer->header) {
		timer_tick_prepare(timer);
	}

	spinlock_unlock(&curr_cpu->timer_lock);
}

/** Cancel a running timer.
 * @param timer		Timer to stop. */
void timer_stop(timer_t *timer) {
	timer_t *next;

	if(!list_empty(&timer->header)) {
		assert(timer->cpu);

		spinlock_lock(&timer->cpu->timer_lock);

		/* Readjust the tick length if required. */
		next = list_entry(timer->cpu->timers.next, timer_t, header);
		if(next == timer && timer->header.next != &timer->cpu->timers) {
			next = list_entry(timer->header.next, timer_t, header);
			timer_tick_prepare(next);
		}

		list_remove(&timer->header);
		spinlock_unlock(&timer->cpu->timer_lock);
	}
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to spin for. */
void spin(useconds_t us) {
	useconds_t target = time_since_boot() + us;
	while(time_since_boot() < target) {
		__asm__ volatile("pause");
	}
}

/** Sleep for a certain amount of time.
 * @param us		Microseconds to sleep for.
 * @param flags		Flags modifying sleep behaviour (see sync/flags.h).
 * @return		0 on success, negative error code on failure. */
int usleep_etc(useconds_t us, int flags) {
	waitq_t queue;
	int ret;

	assert(us >= 0);

	waitq_init(&queue, "usleep");
	ret = waitq_sleep(&queue, us, flags);
	return (ret == -ERR_WOULD_BLOCK || ret == -ERR_TIMED_OUT) ? 0 : ret;
}

/** Sleep for a certain amount of time.
 * @param us		Microseconds to sleep for. */
void usleep(useconds_t us) {
	usleep_etc(us, 0);
}

/** Dump a list of timers.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */ 
int kdbg_cmd_timers(int argc, char **argv) {
	unative_t id = curr_cpu->id;
	timer_t *timer;
	cpu_t *cpu;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<CPU ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all timers on a CPU. If no ID given, current CPU\n");
		kprintf(LOG_NONE, "will be used.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(argc == 2) {
		if(kdbg_parse_expression(argv[1], &id, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}
	}

	if(id > cpu_id_max || !(cpu = cpus[id])) {
		kprintf(LOG_NONE, "Invalid CPU ID.\n");
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "Target           Function           Data\n");
	kprintf(LOG_NONE, "======           ========           ====\n");
	LIST_FOREACH(&cpu->timers, iter) {
		timer = list_entry(iter, timer_t, header);
		kprintf(LOG_NONE, "%-16llu %-18p %p\n", timer->target, timer->func, timer->data);
	}

	return KDBG_OK;
}

/** Print the system uptime.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */ 
int kdbg_cmd_uptime(int argc, char **argv) {
	useconds_t time = time_since_boot();

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints how many microseconds have passed since the kernel started.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "%llu seconds (%llu microseconds)\n", time / 1000000, time);
	return KDBG_OK;
}

/** Initialise the timing system. */
void __init_text time_init(void) {
	time_arch_init();

	/* Initialise the boot time. */
	boot_unix_time = time_from_hardware() - time_since_boot();
}
