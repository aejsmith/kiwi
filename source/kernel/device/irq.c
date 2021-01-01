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

struct irq_handler {
    list_t header;                  /**< List header. */

    unsigned num;                   /**< IRQ number. */
    irq_early_func_t early_func;    /**< Early handler function. */
    irq_func_t func;                /**< Threaded handler function. */
    void *data;                     /**< Argument to pass to handler. */
    bool thread_pending;            /**< Whether execution of threaded handler is pending. */
};

/** An entry in the IRQ table. */
typedef struct irq {
    spinlock_t handlers_lock;       /**< Lock to protect handler list. */
    list_t handlers;                /**< List of handler structures. */
    unsigned int disable_count;     /**< Reference count for disabling the IRQ. */

    mutex_t thread_lock;            /**< Lock for IRQ thread. */
    thread_t *thread;               /**< Thread for deferred handling. */
    semaphore_t sem;                /**< Semaphore for thread to wait for IRQs on. */
    unsigned threaded_handlers;     /**< Number of threaded handlers. */
} irq_t;

/** IRQ controller being used. */
static irq_controller_t *irq_controller;

/** Array of IRQ structures. */
static irq_t irq_table[IRQ_COUNT];

static void enable_irq(irq_t *irq, unsigned num) {
    assert(irq->disable_count > 0);

    if (--irq->disable_count == 0 && irq_controller->enable)
        irq_controller->enable(num);
}

static void disable_irq(irq_t *irq, unsigned num) {
    if (irq->disable_count++ == 0 && irq_controller->disable)
        irq_controller->disable(num);
}

static void irq_thread(void *_num, void *arg2) {
    unsigned num = (ptr_t)_num;
    irq_t *irq = &irq_table[num];

    while (true) {
        semaphore_down(&irq->sem);

        /* Check if we should exit (see irq_unregister()). */
        if (irq->threaded_handlers == 0) {
            mutex_lock(&irq->thread_lock);

            bool exit = irq->threaded_handlers == 0;
            if (exit) {
                thread_release(irq->thread);
                irq->thread = NULL;
            }

            mutex_unlock(&irq->thread_lock);

            if (exit)
                return;
        }

        /* Find the first handler with a pending thread execution. */
        spinlock_lock(&irq->handlers_lock);

        bool found = false;
        list_foreach(&irq->handlers, iter) {
            irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

            found = handler->thread_pending;
            if (found) {
                handler->thread_pending = false;

                /* Can't execute handler with spinlock held. */
                spinlock_unlock(&irq->handlers_lock);

                handler->func(num, handler->data);

                /* We only execute one handler per iteration. Since we unlocked,
                 * the list might have changed underneath us. If more handlers
                 * need to be executed, we should continue for another iteration
                 * as the semaphore is incremented for each pending handler. */
                break;
            }
        }

        /* Re-enable the IRQ. */
        if (found) {
            spinlock_lock(&irq->handlers_lock);

            enable_irq(irq, num);
        }

        spinlock_unlock(&irq->handlers_lock);
    }
}

/**
 * Registers a handler for an IRQ. The new handler will be appended to the
 * list of handlers for the IRQ (IRQ handlers are called in the order they are
 * registered in).
 *
 * See the description of irq_early_func_t and irq_func_t for details of when
 * each handler is executed and recommendations for usage.
 *
 * When an IRQ fires, the IRQ will be disabled until all handlers have finished
 * executing.
 *
 * @param num           IRQ number.
 * @param early_func    Early handler function, see irq_early_func_t. Can be
 *                      null, but must be non-null if func is null.
 * @param func          Main, threaded, handler function, see irq_func_t. Can
 *                      be null, but must be non-null if early_func is null.
 * @param data          Data argument to pass to the handlers.
 * @param _handler      Where to store handler pointer, to be passed to
 *                      irq_unregister() to unregister the handler.
 *
 * @return              Status code describing result of the operation.
 */
status_t irq_register(
    unsigned num, irq_early_func_t early_func, irq_func_t func, void *data,
    irq_handler_t **_handler)
{
    if (num >= IRQ_COUNT || (!func && !early_func))
        return STATUS_INVALID_ARG;

    irq_handler_t *handler = kmalloc(sizeof(irq_handler_t), MM_KERNEL);

    list_init(&handler->header);

    handler->num            = num;
    handler->early_func     = early_func;
    handler->func           = func;
    handler->data           = data;
    handler->thread_pending = false;

    irq_t *irq = &irq_table[num];

    /* Create a handler thread if we need it and there isn't already one. */
    if (handler->func) {
        mutex_lock(&irq->thread_lock);

        irq->threaded_handlers++;

        if (!irq->thread) {
            char name[THREAD_NAME_MAX];
            sprintf(name, "irq-%u", num);

            status_t ret = thread_create(name, NULL, 0, irq_thread, (void *)((ptr_t)num), NULL, &irq->thread);
            if (ret != STATUS_SUCCESS) {
                irq->threaded_handlers--;

                mutex_unlock(&irq->thread_lock);
                kfree(handler);
                return ret;
            }

            thread_run(irq->thread);
        }

        mutex_unlock(&irq->thread_lock);
    }

    spinlock_lock(&irq->handlers_lock);

    bool enable = list_empty(&irq->handlers);
    list_append(&irq->handlers, &handler->header);

    /* Enable it if the list was empty before. */
    if (enable) {
        assert(irq->disable_count == 1);

        enable_irq(irq, num);
    }

    spinlock_unlock(&irq->handlers_lock);

    if (_handler)
        *_handler = handler;

    return STATUS_SUCCESS;
}

/** Removes a previously registered handler for an IRQ.
 * @param handler       Handler returned from irq_register(). */
void irq_unregister(irq_handler_t *handler) {
    irq_t *irq = &irq_table[handler->num];

    /* Stop the thread if this is the last threaded handler for this IRQ. The
     * thread function will check the threaded handler count when it is woken
     * and exit if it is 0. It is left up to the thread to release itself and
     * set the thread pointer null, so that if another threaded handler is
     * registered between here and the thread being woken, it won't exit. */
    if (handler->func) {
        mutex_lock(&irq->thread_lock);

        irq->threaded_handlers--;
        semaphore_up(&irq->sem, 1);

        mutex_unlock(&irq->thread_lock);
    }

    spinlock_lock(&irq->handlers_lock);

    list_remove(&handler->header);

    /*
     * If the list is now empty, the IRQ should be disabled.
     *
     * If there is a thread execution still pending for this handler, the IRQ
     * will have been disabled. Since the thread won't process this now, we
     * should actually enable it if there are still other handlers remaining.
     */
    bool disable = list_empty(&irq->handlers) && !handler->thread_pending;
    bool enable  = !list_empty(&irq->handlers) && handler->thread_pending;

    if (disable) {
        disable_irq(irq, handler->num);
    } else if (enable) {
        enable_irq(irq, handler->num);
    }

    assert(!list_empty(&irq->handlers) || irq->disable_count == 1);

    spinlock_unlock(&irq->handlers_lock);

    kfree(handler);
}

typedef struct device_irq_resource {
    irq_handler_t *handler;
} device_irq_resource_t;

static void device_irq_resource_release(device_t *device, void *data) {
    device_irq_resource_t *resource = data;

    irq_unregister(resource->handler);
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
    device_t *device, unsigned num, irq_early_func_t early_func,
    irq_func_t func, void *data)
{
    irq_handler_t *handler;
    status_t ret = irq_register(num, early_func, func, data, &handler);
    if (ret != STATUS_SUCCESS)
        return ret;

    device_irq_resource_t *resource = device_resource_alloc(
        sizeof(device_irq_resource_t), device_irq_resource_release, MM_KERNEL);

    resource->handler = handler;

    device_resource_register(device, resource);

    return STATUS_SUCCESS;
}

static bool wake_irq_thread(irq_t *irq, irq_handler_t *handler) {
    assert(handler->func);

    bool disable = false;

    if (!handler->thread_pending) {
        handler->thread_pending = true;

        semaphore_up(&irq->sem, 1);
        curr_cpu->should_preempt = true;

        if (irq->disable_count++ == 0)
            disable = true;
    }

    return disable;
}

/** Hardware interrupt handler.
 * @param num           IRQ number. */
void irq_handler(unsigned num) {
    assert(irq_controller);
    assert(num < IRQ_COUNT);

    /* IRQs can happen during a user memory operation. Force the flag to off
     * while handling an IRQ so that we don't incorrectly treat faults during
     * the handler as a user memory violation. */
    bool in_usermem = curr_thread->in_usermem;
    curr_thread->in_usermem = false;

    /* Execute any pre-handling function. */
    if (irq_controller->pre_handle && !irq_controller->pre_handle(num))
        goto out;

    /* Get the trigger mode. */
    irq_mode_t mode = irq_controller->mode(num);

    irq_t *irq = &irq_table[num];
    spinlock_lock(&irq->handlers_lock);

    bool disable = false;

    /* First see if any early handlers take the IRQ. */
    list_foreach(&irq->handlers, iter) {
        irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

        if (handler->early_func) {
            irq_status_t ret = handler->early_func(num, handler->data);

            switch (ret) {
                case IRQ_PREEMPT:
                    curr_cpu->should_preempt = true;
                    break;
                case IRQ_RUN_THREAD:
                    disable |= wake_irq_thread(irq, handler);
                    break;
                default:
                    break;
            }

            /* For edge-triggered interrupts we must invoke all handlers,
             * because multiple interrupt pulses can be merged if they occur
             * close together. */
            if (mode == IRQ_MODE_LEVEL && ret != IRQ_UNHANDLED)
                goto out_handled;
        }
    }

    /* No early handlers took the IRQ, or the IRQ is edge-triggered. Run all
     * handlers without early handlers. */
    list_foreach(&irq->handlers, iter) {
        irq_handler_t *handler = list_entry(iter, irq_handler_t, header);

        if (!handler->early_func)
            disable |= wake_irq_thread(irq, handler);
    }

out_handled:
    spinlock_unlock(&irq->handlers_lock);

    /* Perform post-handling actions. IRQ is disabled until the thread completes
     * execution of all handlers. */
    if (irq_controller->post_handle)
        irq_controller->post_handle(num, disable);

out:
    curr_thread->in_usermem = in_usermem;
}

/** Initialize the IRQ handling system.
 * @param controller    IRQ controller to use. */
__init_text void irq_init(irq_controller_t *controller) {
    assert(controller->mode);

    for (unsigned i = 0; i < IRQ_COUNT; i++) {
        irq_t *irq = &irq_table[i];

        spinlock_init(&irq->handlers_lock, "irq_handlers_lock");
        list_init(&irq->handlers);
        mutex_init(&irq->thread_lock, "irq_thread_lock", 0);
        semaphore_init(&irq->sem, "irq_thread_sem", 0);

        /* Start disabled until a handler is registered. */
        irq->disable_count = 1;
    }

    irq_controller = controller;
}
