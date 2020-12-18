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
 * @brief               Hardware interrupt handling code.
 */

#include <device/device.h>
#include <device/irq.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <platform/irq.h>

#include <proc/thread.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <assert.h>
#include <cpu.h>
#include <kernel.h>
#include <status.h>

/** Structure describing a handler for an IRQ. */
typedef struct irq_handler {
    list_t header;              /**< List header. */

    unsigned num;               /**< IRQ number. */
    irq_top_t top;              /**< Top-half handler. */
    irq_bottom_t bottom;        /**< Bottom-half handler. */
    void *data;                 /**< Argument to pass to handler. */

    thread_t *thread;           /**< Thread for deferred handling. */
    semaphore_t sem;            /**< Semaphore to wait for interrupts on. */
} irq_handler_t;

/** An entry in the IRQ table. */
typedef struct irq {
    spinlock_t lock;            /**< Lock to protect handler list. */
    list_t handlers;            /**< List of handler structures. */
} irq_t;

/** IRQ controller being used. */
static irq_controller_t *irq_controller;

/** Array of IRQ structures. */
static irq_t irq_table[IRQ_COUNT];

static void irq_thread(void *_handler, void *arg2) {
    irq_handler_t *handler = _handler;

    assert(handler->bottom);

    while (true) {
        semaphore_down(&handler->sem);

        /* If the list header is not attached the handler has been removed,
         * destroy it and exit. */
        if (list_empty(&handler->header)) {
            kfree(handler);
            return;
        }

        handler->bottom(handler->num, handler->data);
    }
}

/**
 * Registers a handler for an IRQ. The new handler will be appended to the
 * list of IRQ handlers (IRQ handlers are called in the order they are
 * registered in. The top-half handler will be run when the IRQ occurs. If it
 * returns IRQ_RUN_THREAD, then the bottom-half handler will be executed inside
 * a dedicated handler thread. If no top-half handler is specified then the
 * bottom-half handler will always be run.
 *
 * @param num           IRQ number.
 * @param top           Top-half handler, called in interrupt context.
 * @param bottom        Bottom-half handler, run in a seperate thread if
 *                      requested by the top-half handler.
 * @param data          Data argument to pass to the handlers.
 *
 * @return              Status code describing result of the operation.
 */
status_t irq_register(unsigned num, irq_top_t top, irq_bottom_t bottom, void *data) {
    if (num >= IRQ_COUNT || (!top && !bottom))
        return STATUS_INVALID_ARG;

    irq_handler_t *handler = kmalloc(sizeof(irq_handler_t), MM_KERNEL);

    list_init(&handler->header);
    semaphore_init(&handler->sem, "irq_sem", 0);

    handler->num    = num;
    handler->top    = top;
    handler->bottom = bottom;
    handler->data   = data;
    handler->thread = NULL;

    /* Create a handler thread if necessary. */
    if (handler->bottom) {
        char name[THREAD_NAME_MAX];
        sprintf(name, "irq-%u", num);

        status_t ret = thread_create(name, NULL, 0, irq_thread, handler, NULL, &handler->thread);
        if (ret != STATUS_SUCCESS) {
            kfree(handler);
            return ret;
        }
    }

    spinlock_lock(&irq_table[num].lock);

    /* Check if a handler exists with the same functions/data. */
    list_foreach(&irq_table[num].handlers, iter) {
        irq_handler_t *exist = list_entry(iter, irq_handler_t, header);

        if (exist->top == top && exist->bottom == bottom && exist->data == data) {
            spinlock_unlock(&irq_table[num].lock);

            if (handler->thread)
                thread_release(handler->thread);

            kfree(handler);
            return STATUS_ALREADY_EXISTS;
        }
    }

    bool enable = list_empty(&irq_table[num].handlers);

    list_append(&irq_table[num].handlers, &handler->header);

    /* Enable it if the list was empty before. */
    if (enable && irq_controller->enable)
        irq_controller->enable(num);

    spinlock_unlock(&irq_table[num].lock);

    /* Run the thread. */
    if (handler->thread)
        thread_run(handler->thread);

    return STATUS_SUCCESS;
}

/**
 * Removes a previously registered handler for an IRQ. This function must be
 * given the handler functions/data argument the handler was originally
 * registered with in order to be able to find the correct handler to remove.
 *
 * @param num           IRQ number.
 * @param top           Top-half function handler was registered with.
 * @param bottom        Bottom-half function handler was registered with.
 * @param data          Data argument handler was registered with.
 *
 * @return              Status code describing result of the operation.
 */
status_t irq_unregister(unsigned num, irq_top_t top, irq_bottom_t bottom, void *data) {
    if (num >= IRQ_COUNT)
        return STATUS_INVALID_ARG;

    spinlock_lock(&irq_table[num].lock);

    list_foreach(&irq_table[num].handlers, iter) {
        irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

        if (handler->top != top || handler->bottom != bottom || handler->data != data)
            continue;

        list_remove(&handler->header);

        /* Disable if list now empty. */
        if (list_empty(&irq_table[num].handlers) && irq_controller->disable)
            irq_controller->disable(num);

        spinlock_unlock(&irq_table[num].lock);

        /* If the handler has a thread, we leave destruction of the structure
         * up to the thread - it checks whether the list header is attached
         * each time it is woken to determine if it should exit. */
        if (handler->thread) {
            semaphore_up(&handler->sem, 1);
            thread_release(handler->thread);
        } else {
            kfree(handler);
        }

        return STATUS_SUCCESS;
    }

    spinlock_unlock(&irq_table[num].lock);
    return STATUS_NOT_FOUND;
}

typedef struct device_irq_resource {
    unsigned num;
    irq_top_t top;
    irq_bottom_t bottom;
    void *data;
} device_irq_resource_t;

static void device_irq_resource_release(device_t *device, void *data) {
    device_irq_resource_t *resource = data;

    irq_unregister(resource->num, resource->top, resource->bottom, resource->data);
}

/**
 * Register an IRQ handler as a device-managed resource (will be unregistered
 * when the device is destroyed).
 *
 * @see                 irq_register().
 *
 * @param device        Device to register to.
 */
status_t device_irq_register(
    struct device *device, unsigned num, irq_top_t top, irq_bottom_t bottom,
    void *data)
{
    status_t ret = irq_register(num, top, bottom, data);
    if (ret != STATUS_SUCCESS)
        return ret;

    device_irq_resource_t *resource = device_resource_alloc(
        sizeof(device_irq_resource_t), device_irq_resource_release, MM_KERNEL);

    resource->num    = num;
    resource->top    = top;
    resource->bottom = bottom;
    resource->data   = data;

    device_resource_register(device, resource);

    return STATUS_SUCCESS;
}

/** Hardware interrupt handler.
 * @param num           IRQ number. */
void irq_handler(unsigned num) {
    assert(irq_controller);
    assert(num < IRQ_COUNT);

    /* Execute any pre-handling function. */
    if (irq_controller->pre_handle && !irq_controller->pre_handle(num))
        return;

    /* Get the trigger mode. */
    irq_mode_t mode = irq_controller->mode(num);

    /* First see if any handlers with top-half functions take the IRQ. */
    list_foreach_safe(&irq_table[num].handlers, iter) {
        irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

        if (handler->top) {
            irq_status_t ret = handler->top(num, handler->data);

            switch (ret) {
                case IRQ_PREEMPT:
                    curr_cpu->should_preempt = true;
                    break;
                case IRQ_RUN_THREAD:
                    assert(handler->thread);

                    semaphore_up(&handler->sem, 1);
                    curr_cpu->should_preempt = true;
                    break;
                default:
                    break;
            }

            /* For edge-triggered interrupts we must invoke all handlers,
             * because multiple interrupt pulses can be merged if they occur
             * close together. */
            if (mode == IRQ_MODE_LEVEL && ret != IRQ_UNHANDLED)
                goto out;
        }
    }

    /* No top-half handlers took the IRQ, or the IRQ is edge-triggered. Run all
     * handlers without top-half functions. */
    list_foreach_safe(&irq_table[num].handlers, iter) {
        irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

        if (!handler->top) {
            assert(handler->thread);

            semaphore_up(&handler->sem, 1);
            curr_cpu->should_preempt = true;
        }
    }

out:
    /* Perform post-handling actions. */
    if (irq_controller->post_handle)
        irq_controller->post_handle(num);
}

/** Initialize the IRQ handling system.
 * @param ctrlr         IRQ controller to use. */
__init_text void irq_init(irq_controller_t *ctrlr) {
    assert(ctrlr->mode);

    /* Initialize the IRQ table. */
    for (size_t i = 0; i < IRQ_COUNT; i++) {
        spinlock_init(&irq_table[i].lock, "irq_lock");
        list_init(&irq_table[i].handlers);
    }

    /* Set the controller. */
    irq_controller = ctrlr;
}
