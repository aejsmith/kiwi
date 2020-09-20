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
 */

#pragma once

#include <kernel/time.h>
#include <lib/list.h>
#include <types.h>

struct cpu;

/** Convert seconds to nanoseconds.
 * @param secs          Seconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t secs_to_nsecs(nstime_t secs) {
    return secs * 1000000000;
}

/** Convert milliseconds to nanoseconds.
 * @param msecs         Milliseconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t msecs_to_nsecs(nstime_t msecs) {
    return msecs * 1000000;
}

/** Convert microseconds to nanoseconds.
 * @param usecs         Microseconds value to convert.
 * @return              Equivalent time in nanoseconds. */
static inline nstime_t usecs_to_nsecs(nstime_t usecs) {
    return usecs * 1000;
}

/** Convert nanoseconds to seconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in seconds. */
static inline nstime_t nsecs_to_secs(nstime_t nsecs) {
    return nsecs / 1000000000;
}

/** Convert nanoseconds to milliseconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in milliseconds. */
static inline nstime_t nsecs_to_msecs(nstime_t nsecs) {
    return nsecs / 1000000;
}

/** Convert nanoseconds to microseconds.
 * @param nsecs         Nanoseconds value to convert.
 * @return              Equivalent time in microseconds. */
static inline nstime_t nsecs_to_usecs(nstime_t nsecs) {
    return nsecs / 1000;
}

/** Structure containing details of a hardware timer. */
typedef struct timer_device {
    const char *name;               /**< Name of the timer. */

    /** Type of the device. */
    enum {
        TIMER_DEVICE_PERIODIC,      /**< Timer fires periodically. */
        TIMER_DEVICE_ONESHOT,       /**< Timer fires after the period specified. */
    } type;

    /** Enable the device (for periodic devices). */
    void (*enable)(void);

    /** Disable the device (for periodic devices). */
    void (*disable)(void);

    /** Set up the next tick (for one-shot devices).
     * @note                It is OK if the timer fires before the interval is
     *                      up (for example if the time is outside the range
     *                      supported by the timer): the timer tick handler will
     *                      only call handlers if system_time() shows that the
     *                      timer's target has been reached.
     * @param nsecs         Nanoseconds to fire in. */
    void (*prepare)(nstime_t nsecs);
} timer_device_t;

/** Callback function for timers.
 * @warning             Unless TIMER_THREAD is specified in the timer's flags,
 *                      this function is called in interrupt context. Be
 *                      careful!
 * @param data          Data argument from timer creator.
 * @return              Whether to preempt the current thread after handling.
 *                      This is ignored if the function is run in thread
 *                      context. */
typedef bool (*timer_func_t)(void *data);

/** Structure containing details of a timer. */
typedef struct timer {
    list_t header;                  /**< Link to timers list. */

    nstime_t target;                /**< Time at which the timer will fire. */
    struct cpu *cpu;                /**< CPU that the timer was started on. */
    timer_func_t func;              /**< Function to call when the timer expires. */
    void *data;                     /**< Argument to pass to timer handler. */
    uint32_t flags;                 /**< Behaviour flags. */
    unsigned mode;                  /**< Mode of the timer. */
    nstime_t initial;               /**< Initial time (for periodic timers). */
    const char *name;               /**< Name of the timer (for debugging purposes). */
} timer_t;

/** Behaviour flags for timers. */
#define TIMER_THREAD        (1<<0)  /**< Run the handler in thread (DPC) context. */

extern nstime_t time_to_unix(
    unsigned year, unsigned month, unsigned day, unsigned hour, unsigned min,
    unsigned sec);

extern nstime_t system_time(void);
extern nstime_t unix_time(void);
extern nstime_t boot_time(void);

extern void timer_device_set(timer_device_t *device);
extern bool timer_tick(void);
extern void timer_init(
    timer_t *timer, const char *name, timer_func_t func, void *data,
    uint32_t flags);
extern void timer_start(timer_t *timer, nstime_t length, unsigned mode);
extern void timer_stop(timer_t *timer);

extern status_t delay_etc(nstime_t nsecs, int flags);
extern void delay(nstime_t nsecs);
extern void spin(nstime_t nsecs);

extern nstime_t platform_time_from_hardware(void);

extern void time_init(void);
extern void time_init_percpu(void);
