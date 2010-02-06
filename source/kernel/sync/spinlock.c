/*
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Spinlock implementation.
 */

#include <arch/barrier.h>
#include <arch/spinlock.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>

/** Lock a spinlock.
 *
 * Attempts to lock the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to lock
 * an already held spinlock on a single-processor system.
 *
 * @param lock		Spinlock to lock.
 * @param flags		Synchronization flags. Only SYNC_NONBLOCK applies.
 *
 * @return		0 if lock was taken (always the case if SYNC_NONBLOCK
 *			not specified), negative error code if not.
 */
int spinlock_lock(spinlock_t *lock, int flags) {
	int state;

	/* Disable interrupts while locked to ensure that nothing else
	 * will run on the current CPU for the duration of the lock. */
	state = intr_disable();

	/* When running on a single processor there is no need for us to
	 * spin as there should only ever be one thing here at any one time.
	 * so just die if it's already locked. */
	if(cpu_count > 1) {
		if(flags & SYNC_NONBLOCK) {
			if(!atomic_cmp_set(&lock->locked, 0, 1)) {
				intr_restore(state);
				return -ERR_WOULD_BLOCK;
			}
		} else {
			while(!atomic_cmp_set(&lock->locked, 0, 1)) {
				spinlock_loop_hint();
			}
		}
	} else {
		if(unlikely(!atomic_cmp_set(&lock->locked, 0, 1))) {
			fatal("Nested locking of spinlock %p (%s)", lock, lock->name);
		}
	}

	lock->state = state;

	enter_cs_barrier();
	return 0;
}

/** Lock a spinlock without changing interrupt state.
 *
 * Attempts to lock the specified spinlock, and spins in a loop until it is
 * able to do so. If the call is made on a single-processor system, then
 * fatal() will be called if the lock is already held rather than spinning -
 * spinlocks disable interrupts while locked so nothing should attempt to lock
 * an already held spinlock on a single-processor system. This function does
 * not modify the interrupt state so the caller must ensure that interrupts
 * are disabled (an assertion is made to ensure that this is the case). The
 * interrupt state field of the lock is not updated, therefore a lock that
 * was locked with this function MUST be unlocked with spinlock_unlock_ni().
 *
 * @param lock		Spinlock to lock.
 * @param flags		Synchronization flags. Only SYNC_NONBLOCK applies.
 *
 * @return		0 if lock was taken (always the case if SYNC_NONBLOCK
 *			not specified), negative error code if not.
 */
int spinlock_lock_ni(spinlock_t *lock, int flags) {
	assert(intr_state() == false);

	/* When running on a single processor there is no need for us to
	 * spin as there should only ever be one thing here at any one time.
	 * so just die if it's already locked. */
	if(cpu_count > 1) {
		if(flags & SYNC_NONBLOCK) {
			if(!atomic_cmp_set(&lock->locked, 0, 1)) {
				return -ERR_WOULD_BLOCK;
			}
		} else {
			while(!atomic_cmp_set(&lock->locked, 0, 1)) {
				spinlock_loop_hint();
			}
		}
	} else {
		if(unlikely(!atomic_cmp_set(&lock->locked, 0, 1))) {
			fatal("Nested locking of spinlock %p (%s)", lock, lock->name);
		}
	}

	enter_cs_barrier();
	return 0;
}

/** Unlock a spinlock.
 *
 * Unlocks the specified spinlock and returns the interrupt state to what it
 * was before the spinlock_lock() call was made.
 *
 * @param lock		Spinlock to unlock.
 */
void spinlock_unlock(spinlock_t *lock) {
	int state;

	if(unlikely(!spinlock_held(lock))) {
		fatal("Unlock of already unlocked spinlock %p (%s)", lock, lock->name);
	}

	/* Save state before unlocking in case it is overwritten by another
	 * waiting CPU. */
	state = lock->state;

	leave_cs_barrier();

	atomic_set(&lock->locked, 0);
	intr_restore(state);
}

/** Unlock a spinlock without changing interrupt state.
 *
 * Unlocks the specified spinlock without modifying the interrupt state. This
 * function should only be used if the lock was originally locked with
 * spinlock_lock_ni().
 *
 * @param lock		Spinlock to unlock.
 */
void spinlock_unlock_ni(spinlock_t *lock) {
	if(unlikely(!spinlock_held(lock))) {
		fatal("Unlock of already unlocked spinlock %p (%s)", lock, lock->name);
	}

	leave_cs_barrier();

	atomic_set(&lock->locked, 0);
}

/** Initialise a spinlock structure.
 *
 * Initialises a spinlock structure and sets it to be unlocked.
 *
 * @param lock		Spinlock to initialise.
 * @param name		Name of the spinlock, used for debugging purposes.
 */
void spinlock_init(spinlock_t *lock, const char *name) {
	atomic_set(&lock->locked, 0);
	lock->name = name;
	lock->state = 0;
}
