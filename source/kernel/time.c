/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#include <cpu/cpu.h>

#include <kernel/time.h>

#include <lib/notifier.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/signal.h>
#include <proc/thread.h>

#include <sync/waitq.h>

#include <assert.h>
#include <console.h>
#include <dpc.h>
#include <kdbg.h>
#include <object.h>
#include <status.h>
#include <time.h>

/** Userspace timer structure. */
typedef struct user_timer {
	object_t obj;			/**< Object header. */

	int flags;			/**< Flags for the timer. */
	timer_t timer;			/**< Kernel timer. */
	notifier_t notifier;		/**< Notifier for the timer event. */
	bool fired;			/**< Whether the event has fired. */
	thread_t *thread;		/**< Thread that created the timer. */
} user_timer_t;

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
		length = timer->target - system_time();
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

/**
 * Get the number of microseconds since the Unix Epoch.
 *
 * Returns the number of microseconds that have passed since the Unix Epoch,
 * 00:00:00 UTC, January 1st, 1970.
 *
 * @return		Number of microseconds since epoch.
 */
useconds_t unix_time(void) {
	return boot_unix_time + system_time();
}

/**
 * Set the current timer device.
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

/** Start a timer, with CPU timer lock held.
 * @param timer		Timer to start. */
static void timer_start_unsafe(timer_t *timer) {
	timer_t *exist;

	assert(list_empty(&timer->header));

	timer->target = system_time() + timer->initial;

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
}

/** DPC function to run a timer function.
 * @param _timer	Pointer to timer. */
static void timer_dpc_request(void *_timer) {
	timer_t *timer = _timer;
	timer->func(timer->data);
}

/** Handles a timer tick.
 * @return		Whether to preempt the current thread. */
bool timer_tick(void) {
	useconds_t time = system_time();
	bool preempt = false;
	timer_t *timer;

	assert(curr_timer_device);

	spinlock_lock(&curr_cpu->timer_lock);

	/* Iterate the list and check for expired timers. */
	LIST_FOREACH_SAFE(&curr_cpu->timers, iter) {
		timer = list_entry(iter, timer_t, header);

		/* Since the list is ordered soonest expiry first, we can break
		 * if the current timer has not expired. */
		if(time < timer->target) {
			break;
		}

		/* Timer has expired, perform its timeout action. */
		list_remove(&timer->header);
		if(timer->flags & TIMER_THREAD) {
			dpc_request(timer_dpc_request, timer);
		} else {
			if(timer->func(timer->data)) {
				preempt = true;
			}
		}

		/* If the timer is periodic, restart it. */
		if(timer->mode == TIMER_PERIODIC) {
			timer_start_unsafe(timer);
		}
	}

	/* Find the next timeout if there is one. */
	if(!list_empty(&curr_cpu->timers)) {
		timer_tick_prepare(list_entry(curr_cpu->timers.next, timer_t, header));
	}

	spinlock_unlock(&curr_cpu->timer_lock);
	return preempt;
}

/** Initialise a timer structure.
 * @param timer		Timer to initialise.
 * @param func		Function to call when the timer expires.
 * @param data		Data argument to pass to timer.
 * @param flags		Behaviour flags for the timer. */
void timer_init(timer_t *timer, timer_func_t func, void *data, int flags) {
	list_init(&timer->header);
	timer->func = func;
	timer->data = data;
	timer->flags = flags;
}

/** Start a timer.
 * @param timer		Timer to start.
 * @param length	Microseconds to run the timer for. If 0 or negative
 *			the function will do nothing.
 * @param mode		Mode for the timer. */
void timer_start(timer_t *timer, useconds_t length, int mode) {
	if(length <= 0) {
		return;
	}

	timer->cpu = curr_cpu;
	timer->mode = mode;
	timer->initial = length;

	spinlock_lock(&curr_cpu->timer_lock);

	timer_start_unsafe(timer);

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

		/* Readjust the tick length if required. We can only do this if
		 * the CPU is the current CPU. If not, it's no big deal: the
		 * CPU will just get a tick and timer_tick() will do nothing. */
		if(timer->cpu == curr_cpu) {
			next = list_entry(timer->cpu->timers.next, timer_t, header);
			if(next == timer && timer->header.next != &timer->cpu->timers) {
				next = list_entry(timer->header.next, timer_t, header);
				timer_tick_prepare(next);
			}
		}

		list_remove(&timer->header);
		spinlock_unlock(&timer->cpu->timer_lock);
	}
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to spin for. */
void spin(useconds_t us) {
	useconds_t target = system_time() + us;
	while(system_time() < target) {
		cpu_spin_hint();
	}
}

/** Sleep for a certain amount of time.
 * @param us		Microseconds to sleep for.
 * @param interruptible	Whether the sleep should be interruptible.
 * @return		Status code describing result of the operation. */
status_t usleep_etc(useconds_t us, bool interruptible) {
	waitq_t queue;
	status_t ret;

	assert(us >= 0);

	waitq_init(&queue, "usleep");
	ret = waitq_sleep(&queue, us, (interruptible) ? SYNC_INTERRUPTIBLE : 0);
	return (ret == STATUS_WOULD_BLOCK || ret == STATUS_TIMED_OUT) ? 0 : ret;
}

/** Sleep for a certain amount of time.
 * @param us		Microseconds to sleep for. */
void usleep(useconds_t us) {
	usleep_etc(us, false);
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

	if(id > highest_cpu_id || !(cpu = cpus[id])) {
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
	useconds_t time = system_time();

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints how much time has passed since the kernel started.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "%llu seconds (%llu microseconds)\n", time / 1000000, time);
	return KDBG_OK;
}

/** Initialise the timing system. */
__init_text void time_init(void) {
	/* Initialise the boot time. */
	boot_unix_time = time_from_hardware() - system_time();
}

/** Closes a handle to a timer.
 * @param handle	Handle being closed. */
static void timer_object_close(object_handle_t *handle) {
	user_timer_t *timer = (user_timer_t *)handle->object;

	notifier_clear(&timer->notifier);
	object_destroy(&timer->obj);
	kfree(timer);
}

/** Signal that a timer is being waited for.
 * @param handle	Handle to timer.
 * @param event		Event to wait for.
 * @param sync		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t timer_object_wait(object_handle_t *handle, int event, void *sync) {
	user_timer_t *timer = (user_timer_t *)handle->object;

	switch(event) {
	case TIMER_EVENT:
		if(timer->fired) {
			timer->fired = false;
			object_wait_signal(sync);
		} else {
			notifier_register(&timer->notifier, object_wait_notifier, sync);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a timer.
 * @param handle	Handle to timer.
 * @param event		Event to wait for.
 * @param sync		Internal data pointer. */
static void timer_object_unwait(object_handle_t *handle, int event, void *sync) {
	user_timer_t *timer = (user_timer_t *)handle->object;

	switch(event) {
	case TIMER_EVENT:
		notifier_unregister(&timer->notifier, object_wait_notifier, sync);
		break;
	}
}

/** Timer object type. */
static object_type_t timer_object_type = {
	.id = OBJECT_TYPE_TIMER,
	.close = timer_object_close,
	.wait = timer_object_wait,
	.unwait = timer_object_unwait,
};

/** Timer handler function for a userspace timer.
 * @param _timer	Pointer to timer.
 * @return		Whether to preempt. */
static bool user_timer_func(void *_timer) {
	user_timer_t *timer = _timer;

	/* Send an alarm signal if required. */
	if(timer->flags & TIMER_SIGNAL) {
		signal_send(timer->thread, SIGALRM, NULL, false);
	}

	/* Signal the event. */
	if(!notifier_run(&timer->notifier, NULL, true)) {
		timer->fired = true;
	}

	return false;
}

/** Create a new timer.
 * @param flags		Flags for the timer.
 * @param handlep	Where to store handle to timer object.
 * @return		Status code describing result of the operation. */
status_t kern_timer_create(int flags, handle_t *handlep) {
	object_acl_t acl;
	object_security_t security = { -1, -1, &acl };
	user_timer_t *timer;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	object_acl_init(&acl);

	timer = kmalloc(sizeof(*timer), MM_SLEEP);
	object_init(&timer->obj, &timer_object_type, &security, NULL);
	timer_init(&timer->timer, user_timer_func, timer, TIMER_THREAD);
	notifier_init(&timer->notifier, timer);
	timer->flags = flags;
	timer->fired = false;
	timer->thread = curr_thread;

	ret = object_handle_create(&timer->obj, NULL, 0, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		object_destroy(&timer->obj);
		kfree(timer);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Start a timer.
 * @param handle	Handle to timer object.
 * @param interval	Interval of the timer in microseconds.
 * @param mode		Mode of the timer. If TIMER_ONESHOT, the timer event
 *			will only be fired once after the specified time period.
 *			If TIMER_PERIODIC, it will be fired periodically at the
 *			specified interval, until timer_stop() is called.
 * @return		Status code describing result of the operation. */
status_t kern_timer_start(handle_t handle, useconds_t interval, int mode) {
	object_handle_t *khandle;
	user_timer_t *timer;
	status_t ret;

	if(interval <= 0 || (mode != TIMER_ONESHOT && mode != TIMER_PERIODIC)) {
		return STATUS_INVALID_ARG;
	}

	ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	timer = (user_timer_t *)khandle->object;
	timer_stop(&timer->timer);
	timer_start(&timer->timer, interval, mode);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Stop a timer.
 * @param handle	Handle to timer object.
 * @param remp		If not NULL, where to store remaining time.
 * @return		Status code describing result of the operation. */
status_t kern_timer_stop(handle_t handle, useconds_t *remp) {
	object_handle_t *khandle;
	user_timer_t *timer;
	useconds_t rem;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	timer = (user_timer_t *)khandle->object;
	if(!list_empty(&timer->timer.header)) {
		timer_stop(&timer->timer);
		if(remp) {
			rem = system_time() - timer->timer.target;
			ret = memcpy_to_user(remp, &rem, sizeof(*remp));
		}
	} else if(remp) {
		ret = memset_user(remp, 0, sizeof(*remp));
	}
	object_handle_release(khandle);
	return ret;
}

/** Get the system time (time since boot).
 * @param usp		Where to store number of microseconds since boot.
 * @return		Status code describing result of the operation. */
status_t kern_system_time(useconds_t *usp) {
	useconds_t ret = system_time();
	return memcpy_to_user(usp, &ret, sizeof(ret));
}

/** Get the time since the UNIX epoch.
 * @param usp		Where to store number of microseconds since the epoch.
 * @return		Status code describing result of the operation. */
status_t kern_unix_time(useconds_t *usp) {
	useconds_t ret = unix_time();
	return memcpy_to_user(usp, &ret, sizeof(ret));
}
