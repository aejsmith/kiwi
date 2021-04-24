/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Interrupt handling code.
 */

#pragma once

#include <types.h>

struct device;

/** IRQ early handler return status. */
typedef enum irq_status {
    /**
     * The IRQ was unrecognised and should be passed on to other handlers, if
     * any. The threaded handler will not be run.
     */
    IRQ_UNHANDLED,

    /**
     * The IRQ was handled by the early handler. The threaded handler will not
     * be run.
     */
    IRQ_HANDLED,

    /**
     * The IRQ was handled by the early handler. The threaded handler will not
     * be run. The current thread should be preempted. Should only be used by
     * timer devices.
     */
    IRQ_PREEMPT,

    /**
     * The IRQ was recognised but not handled. The threaded handler should be
     * run.
     */
    IRQ_RUN_THREAD,
} irq_status_t;

/** IRQ trigger modes. */
typedef enum irq_mode {
    IRQ_MODE_LEVEL,             /**< Level-triggered. */
    IRQ_MODE_EDGE,              /**< Edge-triggered. */
} irq_mode_t;

/** IRQ controller structure. */
typedef struct irq_controller {
    /**
     * Pre-handling function. Called at the start of the hardware IRQ handler
     * before any handlers are called.
     *
     * @param num           IRQ number.
     *
     * @return              True if IRQ should be handled.
     */
    bool (*pre_handle)(unsigned num);

    /**
     * Post-early handling function. Called at the end of the hardware IRQ
     * handler, after any early handlers have been called.
     *
     * @param num           IRQ number.
     * @param disable       Whether the IRQ should be disabled.
     */
    void (*post_handle)(unsigned num, bool disable);

    /** Get IRQ trigger mode.
     * @param num           IRQ number.
     * @return              Trigger mode of the IRQ. */
    irq_mode_t (*mode)(unsigned num);

    /** Enable an IRQ.
     * @param num           IRQ number. */
    void (*enable)(unsigned num);

    /** Disable an IRQ.
     * @param num           IRQ number. */
    void (*disable)(unsigned num);
} irq_controller_t;

/**
 * IRQ early handler function type. This is run in interrupt context directly
 * from the hardware IRQ handler and is therefore limited in what it can do. It
 * is optional, and if not present then a threaded irq_func_t must be present.
 * 
 * An early handler can be used to do filtering of IRQs or some pre-processing,
 * or even handle the IRQ entirely if this is possible within interrupt context.
 * In general it is recommended to use a threaded handler, however, as this is
 * less restrictive and minimises the amount of time we are running with other
 * interrupts disabled.
 *
 * @param num           IRQ number.
 * @param data          Data pointer associated with the handler.
 *
 * @return              IRQ status code.
 */
typedef irq_status_t (*irq_early_func_t)(unsigned num, void *data);

/**
 * IRQ handler function type. This is run in a handler thread and is therefore
 * not limited in what it can do, although it should avoid sleeping for long
 * periods where possible.
 *
 * @param num           IRQ number.
 * @param data          Data pointer associated with the handler.
 */
typedef void (*irq_func_t)(unsigned num, void *data);

/** IRQ handler type (opaque). */
typedef struct irq_handler irq_handler_t;

extern status_t irq_register(
    unsigned num, irq_early_func_t early_func, irq_func_t func, void *data,
    irq_handler_t **_handler);
extern void irq_unregister(irq_handler_t *handler);

extern status_t device_irq_register(
    struct device *device, unsigned num, irq_early_func_t early_func,
    irq_func_t func, void *data);

extern void irq_handler(unsigned num);

extern void irq_set_controller(irq_controller_t *controller);
extern void irq_init(void);
