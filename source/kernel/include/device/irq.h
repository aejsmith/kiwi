/*
 * Copyright (C) 2009-2023 Alex Smith
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

#include <status.h>
#include <types.h>

struct device;
struct irq;
struct irq_domain;

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

/**
 * IRQ domain operations structure. A domain is either a controller or
 * translation domain. Controller domains correspond to actual IRQ controllers,
 * whereas translation domains just map to an IRQ within a destination domain.
 *
 * A domain is treated as a translation domain if the translate() function in
 * this structure is not null. In this case, only that function is used, and
 * and after translating everything will be done through the destination domain.
 */
typedef struct irq_domain_ops {
    /**
     * Translates an IRQ number to another domain. Presence of this indicates
     * that the domain is a translation domain. It is called when registering
     * and unregistering IRQs, and the IRQ will be registered with the
     * destination it returns.
     *
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number.
     * @param _dest_domain  Translated destination IRQ domain.
     * @param _dest_num     Translated destination IRQ number.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*translate)(
        struct irq_domain *domain, uint32_t num, struct irq_domain **_dest_domain,
        uint32_t *_dest_num);

    /**
     * Pre-handling function. Called at the start of the hardware IRQ handler
     * before any handlers are called.
     *
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number.
     *
     * @return              True if IRQ should be handled.
     */
    bool (*pre_handle)(struct irq_domain *domain, uint32_t num);

    /**
     * Post-early handling function. Called at the end of the hardware IRQ
     * handler, after any early handlers have been called.
     *
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number.
     * @param disable       Whether the IRQ should be disabled.
     */
    void (*post_handle)(struct irq_domain *domain, uint32_t num, bool disable);

    /** Get IRQ trigger mode.
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number.
     * @return              Trigger mode of the IRQ. */
    irq_mode_t (*mode)(struct irq_domain *domain, uint32_t num);

    /** Set IRQ trigger mode.
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number.
     * @param mode          Trigger mode for the IRQ. */
    status_t (*set_mode)(struct irq_domain *domain, uint32_t num, irq_mode_t mode);

    /** Enable an IRQ.
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number. */
    void (*enable)(struct irq_domain *domain, uint32_t num);

    /** Disable an IRQ.
     * @param domain        Domain that the IRQ is in.
     * @param num           IRQ number. */
    void (*disable)(struct irq_domain *domain, uint32_t num);
} irq_domain_ops_t;

/**
 * IRQ domain structure. An IRQ domain is a set of IRQ numbers and handlers for
 * those IRQs.
 *
 * Currently this just directly corresponds to an IRQ controller, but this may
 * need to change in future (e.g. DT/OF support interrupt "nexuses" which
 * translate between domains).
 */
typedef struct irq_domain {
    uint32_t count;             /**< Number of IRQs in the domain. */
    irq_domain_ops_t *ops;      /**< Operations for the domain. */
    void *private;              /**< Private data for the domain. */
    struct irq *irqs;           /**< Table of IRQs. */
} irq_domain_t;

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
 * @param data          Data pointer associated with the handler.
 *
 * @return              IRQ status code.
 */
typedef irq_status_t (*irq_early_func_t)(void *data);

/**
 * IRQ handler function type. This is run in a handler thread and is therefore
 * not limited in what it can do, although it should avoid sleeping for long
 * periods where possible.
 *
 * @param data          Data pointer associated with the handler.
 */
typedef void (*irq_func_t)(void *data);

/** IRQ handler type (opaque). */
typedef struct irq_handler irq_handler_t;

extern irq_domain_t *root_irq_domain;

extern status_t irq_set_mode(irq_domain_t *domain, uint32_t num, irq_mode_t mode);
extern status_t irq_register(
    irq_domain_t *domain, uint32_t num,
    irq_early_func_t early_func, irq_func_t func, void *data,
    irq_handler_t **_handler);
extern void irq_unregister(irq_handler_t *handler);

extern status_t device_irq_register(
    struct device *device, uint32_t num, irq_early_func_t early_func,
    irq_func_t func, void *data);

extern void irq_handler(irq_domain_t *domain, uint32_t num);

extern irq_domain_t *irq_domain_create(uint32_t count, irq_domain_ops_t *ops, void *private);

extern void irq_init(void);
