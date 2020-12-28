/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Time handling functions.
 *
 * TODO:
 *  - Timers are tied to the CPU that they are started on. This is the right
 *    thing to do with, e.g. the scheduler timers, but what should we do with
 *    user timers? Load balance them? They'll probably get balanced reasonably
 *    due to thread load balancing. Does it matter that much?
 */

#include <kernel/time.h>

#include <lib/notifier.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <sync/semaphore.h>

#include <assert.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <object.h>
#include <status.h>
#include <time.h>

/** Timer thread state. */
typedef struct timer_thread {
    thread_t *thread;               /**< Thread to execute timers. */
    semaphore_t sem;                /**< Semaphore to wait on. */

    /** List of timers pending execution (protected by cpu_t::timer_lock). */
    list_t timers;
} timer_thread_t;

/** Userspace timer structure. */
typedef struct user_timer {
    mutex_t lock;                   /**< Lock for timer. */
    uint32_t flags;                 /**< Flags for the timer. */
    timer_t timer;                  /**< Kernel timer. */
    notifier_t notifier;            /**< Notifier for the timer event. */
    bool fired;                     /**< Whether the event has fired. */
} user_timer_t;

static inline bool is_leap_year(unsigned year) {
    return ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0));
}

static inline unsigned days_in_year(unsigned year) {
    return (is_leap_year(year)) ? 366 : 365;
}

static unsigned days_before_month[] = {
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

/** The number of nanoseconds since the Epoch the kernel was booted at. */
nstime_t boot_unix_time;

/** Hardware timer device. */
static timer_device_t *timer_device;

/** Convert a date/time to nanoseconds since the epoch. */
nstime_t time_to_unix(
    unsigned year, unsigned month, unsigned day, unsigned hour,
    unsigned min, unsigned sec)
{
    uint32_t seconds = 0;

    /* Start by adding the time of day and day of month together. */
    seconds += sec;
    seconds += min * 60;
    seconds += hour * 60 * 60;
    seconds += (day - 1) * 24 * 60 * 60;

    /* Convert the month into days. */
    seconds += days_before_month[month] * 24 * 60 * 60;

    /* If this year is a leap year, and we're past February, we need to add
     * another day. */
    if (month > 2 && is_leap_year(year))
        seconds += 24 * 60 * 60;

    /* Add the days in each year before this year from 1970. */
    for (unsigned i = 1970; i < year; i++)
        seconds += days_in_year(i) * 24 * 60 * 60;

    return secs_to_nsecs(seconds);
}

/**
 * Gets the number of nanoseconds that have passed since the Unix Epoch,
 * 00:00:00 UTC, January 1st, 1970.
 *
 * @return              Number of nanoseconds since epoch.
 */
nstime_t unix_time(void) {
    return boot_unix_time + system_time();
}

/** Get the system boot time.
 * @return              UNIX time at which the system was booted. */
nstime_t boot_time(void) {
    return boot_unix_time;
}

/** Prepares next timer tick. */
static void timer_device_prepare(timer_t *timer) {
    nstime_t length = timer->target - system_time();

    timer_device->prepare((length > 0) ? length : 1);
}

/** Ensure that the timer device is enabled. */
static inline void timer_device_enable(void) {
    /* The device may not be disabled when we expect it to be (if timer_stop()
     * is run from a different CPU to the one the timer is running on, it won't
     * be able to disable the timer if the list becomes empty). */
    if (!curr_cpu->timer_enabled) {
        timer_device->enable();
        curr_cpu->timer_enabled = true;
    }
}

/** Disable the timer device. */
static inline void timer_device_disable(void) {
    /* The timer device should always be enabled when we expect it to be. */
    assert(curr_cpu->timer_enabled);

    timer_device->disable();
    curr_cpu->timer_enabled = false;
}

/**
 * Sets the device that will provide timer ticks. This function must only be
 * called once.
 *
 * @param device        Device to set.
 */
void timer_device_set(timer_device_t *device) {
    assert(!timer_device);

    timer_device = device;
    if (timer_device->type == TIMER_DEVICE_ONESHOT)
        curr_cpu->timer_enabled = true;

    kprintf(LOG_NOTICE, "timer: activated timer device %s\n", device->name);
}

/** Start a timer, with CPU timer lock held. */
static void timer_start_unsafe(timer_t *timer) {
    assert(list_empty(&timer->cpu_link));

    /* Work out the absolute completion time. */
    timer->target = system_time() + timer->initial;

    /* Find the insertion point for the timer: the list must be ordered with
     * nearest expiration time first. */
    list_t *pos = curr_cpu->timers.next;
    while (pos != &curr_cpu->timers) {
        timer_t *exist = list_entry(pos, timer_t, cpu_link);

        if (exist->target > timer->target)
            break;

        pos = pos->next;
    }

    list_add_before(pos, &timer->cpu_link);
}

static void timer_thread(void *arg1, void *arg2) {
    timer_thread_t *thread = curr_cpu->timer_thread;

    while (true) {
        semaphore_down(&thread->sem);

        spinlock_lock(&curr_cpu->timer_lock);

        /* Timers can be removed before we get a chance to run them. */
        if (!list_empty(&thread->timers)) {
            timer_t *timer = list_first(&thread->timers, timer_t, thread_link);

            /* This prevents it from being freed underneath us. */
            timer->flags |= TIMER_THREAD_RUNNING;
            list_remove(&timer->thread_link);

            spinlock_unlock(&curr_cpu->timer_lock);

            timer->func(timer->data);

            spinlock_lock(&curr_cpu->timer_lock);

            timer->flags &= ~TIMER_THREAD_RUNNING;
        }

        spinlock_unlock(&curr_cpu->timer_lock);
    }
}

/** Handles a timer tick.
 * @return              Whether to preempt the current thread. */
bool timer_tick(void) {
    assert(timer_device);
    assert(!local_irq_state());

    if (!curr_cpu->timer_enabled)
        return false;

    nstime_t time = system_time();

    spinlock_lock(&curr_cpu->timer_lock);

    bool preempt = false;

    /* Iterate the list and check for expired timers. */
    list_foreach_safe(&curr_cpu->timers, iter) {
        timer_t *timer = list_entry(iter, timer_t, cpu_link);

        /* Since the list is ordered soonest expiry first, we can break if the
         * current timer has not expired. */
        if (time < timer->target)
            break;

        /* This timer has expired, remove it from the list. */
        list_remove(&timer->cpu_link);

        /* Perform its timeout action. */
        if (timer->flags & TIMER_THREAD) {
            list_append(&curr_cpu->timer_thread->timers, &timer->thread_link);
            semaphore_up(&curr_cpu->timer_thread->sem, 1);
        } else {
            if (timer->func(timer->data))
                preempt = true;
        }

        /* If the timer is periodic, restart it. */
        if (timer->mode == TIMER_PERIODIC)
            timer_start_unsafe(timer);
    }

    switch (timer_device->type) {
        case TIMER_DEVICE_ONESHOT:
            /* Prepare the next tick if there is still a timer in the list. */
            if (!list_empty(&curr_cpu->timers))
                timer_device_prepare(list_first(&curr_cpu->timers, timer_t, cpu_link));

            break;
        case TIMER_DEVICE_PERIODIC:
            /* For periodic devices, if the list is empty disable the device so the
            * timer does not interrupt unnecessarily. */
            if (list_empty(&curr_cpu->timers))
                timer_device_disable();

            break;
    }

    spinlock_unlock(&curr_cpu->timer_lock);
    return preempt;
}

/** Initialize a timer structure.
 * @param timer         Timer to initialize.
 * @param name          Name of the timer for debugging purposes.
 * @param func          Function to call when the timer expires.
 * @param data          Data argument to pass to timer.
 * @param flags         Behaviour flags for the timer. */
void timer_init(timer_t *timer, const char *name, timer_func_t func, void *data, uint32_t flags) {
    list_init(&timer->cpu_link);
    list_init(&timer->thread_link);

    timer->func  = func;
    timer->data  = data;
    timer->flags = flags;
    timer->name  = name;
}

/** Start a timer.
 * @param timer         Timer to start. Must not already be running.
 * @param length        Nanoseconds to run the timer for. If 0 or negative
 *                      the function will do nothing.
 * @param mode          Mode for the timer. */
void timer_start(timer_t *timer, nstime_t length, unsigned mode) {
    if (length <= 0)
        return;

    /* Prevent curr_cpu from changing underneath us. */
    bool irq_state = local_irq_disable();

    timer->cpu     = curr_cpu;
    timer->mode    = mode;
    timer->initial = length;

    spinlock_lock_noirq(&curr_cpu->timer_lock);

    /* Add the timer to the list. */
    timer_start_unsafe(timer);

    switch (timer_device->type) {
        case TIMER_DEVICE_ONESHOT:
            /* If the new timer is at the beginning of the list, then it has
             * the shortest remaining time, so we need to adjust the device to
             * tick for it. */
            if (timer == list_first(&curr_cpu->timers, timer_t, cpu_link))
                timer_device_prepare(timer);

            break;
        case TIMER_DEVICE_PERIODIC:
            /* Enable the device. */
            timer_device_enable();
            break;
    }

    spinlock_unlock_noirq(&curr_cpu->timer_lock);
    local_irq_restore(irq_state);
}

/** Cancel a running timer.
 * @param timer         Timer to stop. */
void timer_stop(timer_t *timer) {
    if (!list_empty(&timer->cpu_link)) {
        assert(timer->cpu);

        spinlock_lock(&timer->cpu->timer_lock);

        timer_t *first = list_first(&timer->cpu->timers, timer_t, cpu_link);

        list_remove(&timer->cpu_link);

        /* If the timer is running on this CPU, adjust the tick length or
         * disable the device if required. If the timer is on another CPU, it's
         * no big deal: the tick handler is able to handle unexpected ticks. */
        if (timer->cpu == curr_cpu) {
            switch (timer_device->type) {
                case TIMER_DEVICE_ONESHOT:
                    if (first == timer && !list_empty(&curr_cpu->timers)) {
                        first = list_first(&curr_cpu->timers, timer_t, cpu_link);
                        timer_device_prepare(first);
                    }

                    break;
                case TIMER_DEVICE_PERIODIC:
                    if (list_empty(&curr_cpu->timers))
                        timer_device_disable();

                    break;
            }
        }

        /* If it's pending execution on thread we need to remove it, but make
         * sure we do not return if the thread is currently executing the
         * handler, as the timer owner might free it once we return. */
        list_remove(&timer->thread_link);
        while (timer->flags & TIMER_THREAD_RUNNING) {
            spinlock_unlock(&timer->cpu->timer_lock);
            thread_yield();
            spinlock_lock(&timer->cpu->timer_lock);
        }

        spinlock_unlock(&timer->cpu->timer_lock);
    }
}

/** Sleep for a certain amount of time.
 * @param nsecs         Nanoseconds to sleep for (must be greater than or
 *                      equal to 0). If SLEEP_ABSOLUTE is specified, this is
 *                      a target system time to sleep until.
 * @param flags         Behaviour flags (see sync/sync.h).
 * @return              STATUS_SUCCESS on success, STATUS_INTERRUPTED if
 *                      SLEEP_INTERRUPTIBLE specified and sleep was interrupted. */
status_t delay_etc(nstime_t nsecs, int flags) {
    assert(nsecs >= 0);

    status_t ret = thread_sleep(NULL, nsecs, "delay", flags);
    if (likely(ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK))
        ret = STATUS_SUCCESS;

    return ret;
}

/** Delay for a period of time.
 * @param nsecs         Nanoseconds to sleep for (must be greater than or
 *                      equal to 0). */
void delay(nstime_t nsecs) {
    delay_etc(nsecs, 0);
}

/** Dump a list of timers. */
static kdb_status_t kdb_cmd_timers(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<CPU ID>]\n\n", argv[0]);

        kdb_printf("Prints a list of all timers on a CPU. If no ID given, current CPU\n");
        kdb_printf("will be used.\n");
        return KDB_SUCCESS;
    } else if (argc != 1 && argc != 2) {
        kdb_printf("Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    cpu_t *cpu;
    if (argc == 2) {
        uint64_t id;
        if (kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        if (id > highest_cpu_id || !cpus[id]) {
            kdb_printf("Invalid CPU ID.\n");
            return KDB_FAILURE;
        }

        cpu = cpus[id];
    } else {
        cpu = curr_cpu;
    }

    kdb_printf("Name                 Target           Function           Data\n");
    kdb_printf("====                 ======           ========           ====\n");

    list_foreach(&cpu->timers, iter) {
        timer_t *timer = list_entry(iter, timer_t, cpu_link);

        kdb_printf(
            "%-20s %-16llu %-18p %p\n",
            timer->name, timer->target, timer->func, timer->data);
    }

    return KDB_SUCCESS;
}

/** Print the system uptime. */
static kdb_status_t kdb_cmd_uptime(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints how much time has passed since the kernel started.\n");
        return KDB_SUCCESS;
    }

    nstime_t time = system_time();
    kdb_printf("%llu seconds (%llu nanoseconds)\n", nsecs_to_secs(time), time);
    return KDB_SUCCESS;
}

/** Initialize the timing system. */
__init_text void time_init(void) {
    /* Initialize the boot time. */
    boot_unix_time = platform_time_from_hardware() - system_time();

    /* Register debugger commands. */
    kdb_register_command("timers", "Print a list of running timers.", kdb_cmd_timers);
    kdb_register_command("uptime", "Display the system uptime.", kdb_cmd_uptime);
}

/** Perform late timing system initialization. */
__init_text void time_late_init(void) {
    time_init_percpu();
}

/** Initialize per-CPU time state. */
__init_text void time_init_percpu(void) {
    if (timer_device->type == TIMER_DEVICE_ONESHOT)
        curr_cpu->timer_enabled = true;

    curr_cpu->timer_thread = kmalloc(sizeof(*curr_cpu->timer_thread), MM_BOOT);

    list_init(&curr_cpu->timer_thread->timers);
    semaphore_init(&curr_cpu->timer_thread->sem, "timer_thread_sem", 0);

    char name[THREAD_NAME_MAX];
    sprintf(name, "timer-%u", curr_cpu->id);

    status_t ret = thread_create(name, NULL, 0, timer_thread, NULL, NULL, &curr_cpu->timer_thread->thread);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create timer thread: %d", ret);

    thread_wire(curr_cpu->timer_thread->thread);
    thread_run(curr_cpu->timer_thread->thread);
}

/**
 * User timer API.
 */

/** Closes a handle to a timer. */
static void timer_object_close(object_handle_t *handle) {
    user_timer_t *timer = handle->private;

    timer_stop(&timer->timer);
    notifier_clear(&timer->notifier);
    kfree(timer);
}

/** Signal that a timer is being waited for. */
static status_t timer_object_wait(object_handle_t *handle, object_event_t *event) {
    user_timer_t *timer = handle->private;

    switch (event->event) {
        case TIMER_EVENT:
            if (!(event->flags & OBJECT_EVENT_EDGE) && timer->fired) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&timer->notifier, object_event_notifier, event);
            }

            return STATUS_SUCCESS;
        default:
            return STATUS_INVALID_EVENT;
    }
}

/** Stop waiting for a timer. */
static void timer_object_unwait(object_handle_t *handle, object_event_t *event) {
    user_timer_t *timer = handle->private;

    switch (event->event) {
        case TIMER_EVENT:
            notifier_unregister(&timer->notifier, object_event_notifier, event);
            break;
    }
}

/** Timer object type. */
static object_type_t timer_object_type = {
    .id     = OBJECT_TYPE_TIMER,
    .flags  = OBJECT_TRANSFERRABLE,
    .close  = timer_object_close,
    .wait   = timer_object_wait,
    .unwait = timer_object_unwait,
};

static bool user_timer_func(void *_timer) {
    user_timer_t *timer = _timer;

    if (timer->timer.mode == TIMER_ONESHOT)
        timer->fired = true;

    notifier_run(&timer->notifier, NULL, false);
    return false;
}

/** Create a new timer.
 * @param flags         Flags for the timer.
 * @param _handle       Where to store handle to timer object.
 * @return              Status code describing result of the operation. */
status_t kern_timer_create(uint32_t flags, handle_t *_handle) {
    if (!_handle)
        return STATUS_INVALID_ARG;

    user_timer_t *timer = kmalloc(sizeof(*timer), MM_KERNEL);

    mutex_init(&timer->lock, "user_timer", 0);
    timer_init(&timer->timer, "user_timer", user_timer_func, timer, TIMER_THREAD);
    notifier_init(&timer->notifier, timer);

    timer->flags = flags;
    timer->fired = false;

    status_t ret = object_handle_open(&timer_object_type, timer, NULL, _handle);
    if (ret != STATUS_SUCCESS)
        kfree(timer);

    return ret;
}

/**
 * Starts a timer. A timer can be started in one of two modes. TIMER_ONESHOT
 * will fire the timer event once after the specified time period. After the
 * time period has expired, the timer will remain in the fired state, i.e. a
 * level-triggered wait on the timer event will be immediately signalled, until
 * it is restarted or stopped.
 *
 * TIMER_PERIODIC fires the timer event periodically at the specified interval
 * until it is restarted or stopped. After each event the fired state is
 * cleared, i.e. an edge-triggered wait on the timer will see a rising edge
 * after each period.
 *
 * @param handle        Handle to timer object.
 * @param interval      Interval of the timer in nanoseconds.
 * @param mode          Mode of the timer.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_timer_start(handle_t handle, nstime_t interval, unsigned mode) {
    if (interval <= 0 || (mode != TIMER_ONESHOT && mode != TIMER_PERIODIC))
        return STATUS_INVALID_ARG;

    object_handle_t *khandle;
    status_t ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    user_timer_t *timer = khandle->private;

    mutex_lock(&timer->lock);

    timer_stop(&timer->timer);
    timer->fired = false;
    timer_start(&timer->timer, interval, mode);

    mutex_unlock(&timer->lock);

    object_handle_release(khandle);
    return STATUS_SUCCESS;
}

/** Stops a timer.
 * @param handle        Handle to timer object.
 * @param _rem          If not NULL, where to store remaining time.
 * @return              Status code describing result of the operation. */
status_t kern_timer_stop(handle_t handle, nstime_t *_rem) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_TIMER, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    user_timer_t *timer = khandle->private;

    mutex_lock(&timer->lock);

    if (!list_empty(&timer->timer.cpu_link)) {
        timer_stop(&timer->timer);
        timer->fired = false;

        if (_rem) {
            nstime_t rem = system_time() - timer->timer.target;
            ret = write_user(_rem, rem);
        }
    } else if (_rem) {
        ret = write_user(_rem, 0);
    }

    mutex_unlock(&timer->lock);

    object_handle_release(khandle);
    return ret;
}

/**
 * Gets the current time, in nanoseconds, from the specified time source. There
 * are currently 2 time sources defined:
 *  - TIME_SYSTEM: A monotonic timer which gives the time since the system was
 *    started.
 *  - TIME_REAL: Real time given as time since the UNIX epoch. This can be
 *    changed with kern_time_set().
 *
 * @param source        Time source to get from.
 * @param _time         Where to store time in nanoseconds.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if time source is invalid or _time is
 *                      NULL.
 */
status_t kern_time_get(unsigned source, nstime_t *_time) {
    if (!_time)
        return STATUS_INVALID_ARG;

    nstime_t time;
    switch (source) {
        case TIME_SYSTEM:
            time = system_time();
            break;
        case TIME_REAL:
            time = unix_time();
            break;
        default:
            return STATUS_INVALID_ARG;
    }

    return write_user(_time, time);
}

/**
 * Sets the current time, in nanoseconds, for a time source. Currently only
 * the TIME_REAL source (see kern_time_get()) can be changed.
 *
 * @param source        Time source to set.
 * @param time          New time value in nanoseconds.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if time source is invalid.
 */
status_t kern_time_set(unsigned source, nstime_t time) {
    return STATUS_NOT_IMPLEMENTED;
}
