/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
struct timer_device;

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

/** Operations for a hardware timer. */
typedef struct timer_device_ops {
    /** Type of the device. */
    enum {
        TIMER_DEVICE_PERIODIC,      /**< Timer fires periodically. */
        TIMER_DEVICE_ONESHOT,       /**< Timer fires after the period specified. */
    } type;

    /** Enable the device (for periodic devices). */
    void (*enable)(struct timer_device *device);

    /** Disable the device (for periodic devices). */
    void (*disable)(struct timer_device *device);

    /** Set up the next tick (for one-shot devices).
     * @note                It is OK if the timer fires before the interval is
     *                      up (for example if the time is outside the range
     *                      supported by the timer): the timer tick handler will
     *                      only call handlers if system_time() shows that the
     *                      timer's target has been reached.
     * @param nsecs         Nanoseconds to fire in. */
    void (*prepare)(struct timer_device *device, nstime_t nsecs);
} timer_device_ops_t;

/** Structure containing details of a hardware timer. */
typedef struct timer_device {
    const char *name;               /**< Name of the device. */
    int priority;                   /**< Priority of the device. */
    timer_device_ops_t *ops;        /**< Operations for the device. */
    void *private;                  /**< Private data for the driver. */
} timer_device_t;

/**
 * Callback function for timers. Unless TIMER_THREAD is specified in the
 * timer's flags, this function is called in interrupt context.
 *
 * @param data          Data argument from timer creator.
 *
 * @return              Whether to preempt the current thread after handling.
 *                      This is ignored if the function is run in thread
 *                      context.
 */
typedef bool (*timer_func_t)(void *data);

/** Structure containing details of a timer. */
typedef struct timer {
    list_t cpu_link;                /**< Link to CPU timer list. */
    list_t thread_link;             /**< Link to thread timer link. */

    nstime_t target;                /**< Time at which the timer will fire. */
    struct cpu *cpu;                /**< CPU that the timer was started on. */
    timer_func_t func;              /**< Function to call when the timer expires. */
    void *data;                     /**< Argument to pass to timer handler. */
    uint32_t exec_count;            /**< Number of times the handler has been executed. */
    uint16_t flags;                 /**< Behaviour flags. */
    uint16_t mode;                  /**< Mode of the timer. */
    nstime_t initial;               /**< Initial time (for periodic timers). */
    const char *name;               /**< Name of the timer (for debugging purposes). */
} timer_t;

/** Behaviour flags for timers. */
enum {
    /** Run the handler in thread context. */
    TIMER_THREAD            = (1<<0),

    /** (Internal) thread is currently running the handler. */
    TIMER_THREAD_RUNNING    = (1<<1),
};

extern nstime_t time_to_unix(
    unsigned year, unsigned month, unsigned day, unsigned hour, unsigned min,
    unsigned sec);

extern nstime_t time_from_ticks(uint64_t ticks, uint32_t freq);
extern uint64_t time_to_ticks(nstime_t time, uint32_t freq);

extern nstime_t system_time(void);
extern nstime_t unix_time(void);
extern nstime_t boot_time(void);

extern bool timer_tick(void);
extern void timer_init(
    timer_t *timer, const char *name, timer_func_t func, void *data,
    uint32_t flags);
extern void timer_start(timer_t *timer, nstime_t length, unsigned mode);
extern uint32_t timer_stop(timer_t *timer);

extern status_t delay_etc(nstime_t nsecs, int flags);
extern void delay(nstime_t nsecs);
extern void spin(nstime_t nsecs);

extern nstime_t arch_time_from_hardware(void);

extern bool time_set_timer_device(timer_device_t *device);
extern void time_init(void);
extern void time_late_init(void);
extern void time_init_percpu(void);
