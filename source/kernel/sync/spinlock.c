/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Spinlock implementation.
 */

#include <arch/barrier.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <status.h>

/** Internal spinlock locking code.
 * @param lock		Spinlock to lock.
 * @param timeout	Timeout.
 * @param flags		Synchronization flags.
 * @return		Status code describing result of the operation. */
static inline __always_inline status_t spinlock_lock_internal(spinlock_t *lock, useconds_t timeout, int flags) {
	/* Attempt to take the lock. Prefer the uncontended case. */
	if(unlikely(atomic_dec(&lock->value) != 1)) {
		/* When running on a single processor there is no need for us
		 * to spin as there should only ever be one thing here at any
		 * one time, so just die. */
#if CONFIG_SMP
		if(cpu_count > 1) {
			if(timeout == 0) {
				return STATUS_WOULD_BLOCK;
			}
		
			while(true) {
				/* Wait for it to become unheld. */
				while(atomic_get(&lock->value) != 1) {
					cpu_spin_hint();
				}

				/* Try to acquire it. */
				if(atomic_dec(&lock->value) == 1) {
					break;
				}
			}
		} else {
#endif
			fatal("Nested locking of spinlock %p (%s)", lock, lock->name);
#if CONFIG_SMP
		}
#endif
	}

	return STATUS_SUCCESS;
}

/**
 * Acquire a spinlock.
 *
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to
 * acquire an already held spinlock on a single-processor system.
 *
 * @todo		Timeouts are not implemented yet - only 0 and -1 work.
 *			Anything else is treated as -1.
 *
 * @param lock		Spinlock to acquire.
 * @param timeout	Timeout in microseconds. If 0 is specified, then the
 *			function will return an error if unable to acquire the
 *			lock immediately. If -1, it will spin indefinitely
 *			until able to acquire the lock.
 * @param flags		Synchronization flags (the SYNC_INTERRUPTIBLE flag is
 *			not supported).
 *
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1.
 */
status_t spinlock_lock_etc(spinlock_t *lock, useconds_t timeout, int flags) {
	status_t ret;
	bool state;

	/* Disable interrupts while locked to ensure that nothing else
	 * will run on the current CPU for the duration of the lock. */
	state = intr_disable();

	/* Take the lock. */
	ret = spinlock_lock_internal(lock, timeout, flags);
	if(ret != STATUS_SUCCESS) {
		intr_restore(state);
		return ret;
	}

	lock->state = state;
	enter_cs_barrier();
	return STATUS_SUCCESS;
}

/**
 * Acquire a spinlock without changing interrupt state.
 *
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to
 * acquire an already held spinlock on a single-processor system. This function
 * does not modify the interrupt state so the caller must ensure that
 * interrupts are disabled (an assertion is made to ensure that this is the
 * case). The interrupt state field of the lock is not updated, therefore a
 * lock that was acquired with this function MUST be released with
 * spinlock_unlock_ni().
 *
 * @todo		Timeouts are not implemented yet - only 0 and -1 work.
 *			Anything else is treated as -1.
 *
 * @param lock		Spinlock to acquire.
 * @param timeout	Timeout in microseconds. If 0 is specified, then the
 *			function will return an error if unable to acquire the
 *			lock immediately. If -1, it will spin indefinitely
 *			until able to acquire the lock.
 * @param flags		Synchronization flags (the SYNC_INTERRUPTIBLE flag is
 *			not supported).
 *
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1.
 */
status_t spinlock_lock_ni_etc(spinlock_t *lock, useconds_t timeout, int flags) {
	status_t ret;

	assert(!intr_state());

	/* Take the lock. */
	ret = spinlock_lock_internal(lock, timeout, flags);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	enter_cs_barrier();
	return STATUS_SUCCESS;
}

/**
 * Acquire a spinlock.
 *
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to
 * acquire an already held spinlock on a single-processor system.
 *
 * @param lock		Spinlock to acquire.
 */
void spinlock_lock(spinlock_t *lock) {
	status_t ret;
	bool state;

	/* Disable interrupts while locked to ensure that nothing else
	 * will run on the current CPU for the duration of the lock. */
	state = intr_disable();

	/* Take the lock. */
	ret = spinlock_lock_internal(lock, -1, 0);
	assert(ret == STATUS_SUCCESS);

	lock->state = state;
	enter_cs_barrier();
}

/**
 * Acquire a spinlock without changing interrupt state.
 *
 * Attempts to acquire the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to
 * acquire an already held spinlock on a single-processor system. This function
 * does not modify the interrupt state so the caller must ensure that
 * interrupts are disabled (an assertion is made to ensure that this is the
 * case). The interrupt state field of the lock is not updated, therefore a
 * lock that was acquired with this function MUST be released with
 * spinlock_unlock_ni().
 *
 * @param lock		Spinlock to acquire.
 */
void spinlock_lock_ni(spinlock_t *lock) {
	status_t ret;

	assert(!intr_state());

	/* Take the lock. */
	ret = spinlock_lock_internal(lock, -1, 0);
	assert(ret == STATUS_SUCCESS);

	enter_cs_barrier();
}

/**
 * Release a spinlock.
 *
 * Unlocks the specified spinlock and restores the interrupt state to what it
 * was before the lock was acquired. This should only be used if the lock was
 * acquired using spinlock_lock() or spinlock_lock_etc().
 *
 * @param lock		Spinlock to release.
 */
void spinlock_unlock(spinlock_t *lock) {
	bool state;

	if(unlikely(!spinlock_held(lock))) {
		fatal("Release of already unlocked spinlock %p (%s)", lock, lock->name);
	}

	/* Save state before unlocking in case it is overwritten by another
	 * waiting CPU. */
	state = lock->state;

	leave_cs_barrier();
	atomic_set(&lock->value, 1);
	intr_restore(state);
}

/** Release a spinlock without changing interrupt state.
 * @param lock		Spinlock to release. */
void spinlock_unlock_ni(spinlock_t *lock) {
	if(unlikely(!spinlock_held(lock))) {
		fatal("Release of already unlocked spinlock %p (%s)", lock, lock->name);
	}

	leave_cs_barrier();
	atomic_set(&lock->value, 1);
}

/** Initialise a spinlock structure.
 * @param lock		Spinlock to initialise.
 * @param name		Name of the spinlock, used for debugging purposes. */
void spinlock_init(spinlock_t *lock, const char *name) {
	atomic_set(&lock->value, 1);
	lock->name = name;
	lock->state = false;
}
