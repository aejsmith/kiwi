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
 * @brief               Spinlock implementation.
 */

#include <arch/barrier.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <cpu.h>
#include <status.h>

/** Internal spinlock locking code.
 * @param lock          Spinlock to acquire. */
static inline void spinlock_lock_internal(spinlock_t *lock) {
    /* Attempt to take the lock. Prefer the uncontended case. */
    if (likely(atomic_fetch_sub(&lock->value, 1) == 1))
        return;

    /* When running on a single processor there is no need for us to spin as
     * there should only ever be one thing here at any one time, so just die. */
    if (likely(cpu_count > 1)) {
        while (true) {
            /* Wait for it to become unheld. */
            while (atomic_load(&lock->value) != 1)
                arch_cpu_spin_hint();

            /* Try to acquire it. */
            if (atomic_fetch_sub(&lock->value, 1) == 1)
                break;
        }
    } else {
        fatal("Nested locking of spinlock %p (%s)", lock, lock->name);
    }
}

/**
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so.
 *
 * If the call is made on a single-processor system, then fatal() will be
 * called if the lock is already held rather than spinning - spinlocks disable
 * interrupts while locked so nothing should attempt to acquire an already held
 * spinlock on a single-processor system.
 *
 * @param lock          Spinlock to acquire.
 */
void spinlock_lock(spinlock_t *lock) {
    /* Disable interrupts while locked to ensure that nothing else will run on
     * the current CPU for the duration of the lock. */
    bool irq_state = local_irq_disable();

    spinlock_lock_internal(lock);
    lock->state = irq_state;
}

/**
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so, without changing interrupt state.
 *
 * If the call is made on a single-processor system, then fatal() will be
 * called if the lock is already held rather than spinning - spinlocks disable
 * interrupts while locked so nothing should attempt to acquire an already held
 * spinlock on a single-processor system.
 *
 * This function does not modify the interrupt state so the caller must ensure
 * that interrupts are disabled (an assertion is made to ensure that this is
 * the case). The interrupt state field of the lock is not updated, therefore a
 * lock that was acquired with this function MUST be released with
 * spinlock_unlock_noirq().
 *
 * @param lock          Spinlock to acquire.
 */
void spinlock_lock_noirq(spinlock_t *lock) {
    assert(!local_irq_state());

    spinlock_lock_internal(lock);
}

/**
 * Releases the specified spinlock and restores the interrupt state to what it
 * was before the lock was acquired. This should only be used if the lock was
 * acquired using spinlock_lock().
 *
 * @param lock          Spinlock to release.
 */
void spinlock_unlock(spinlock_t *lock) {
    if (unlikely(!spinlock_held(lock)))
        fatal("Release of already unlocked spinlock %p (%s)", lock, lock->name);

    bool irq_state = lock->state;
    atomic_store(&lock->value, 1);
    local_irq_restore(irq_state);
}

/**
 * Releases the specified spinlock without restoring interrupt state. This must
 * be used if the lock was acquired using spinlock_lock_noirq().
 *
 * @param lock          Spinlock to release.
 */
void spinlock_unlock_noirq(spinlock_t *lock) {
    if (unlikely(!spinlock_held(lock)))
        fatal("Release of already unlocked spinlock %p (%s)", lock, lock->name);

    atomic_store(&lock->value, 1);
}

/** Initializes a spinlock.
 * @param lock          Spinlock to initialize.
 * @param name          Name of the spinlock, used for debugging purposes. */
void spinlock_init(spinlock_t *lock, const char *name) {
    atomic_store_explicit(&lock->value, 1, memory_order_relaxed);
    lock->name  = name;
    lock->state = false;
}
