/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Condition variable code.
 */

#include <sync/condvar.h>

#include <assert.h>

/**
 * Wait for a condition to become true.
 *
 * Atomically unlocks a mutex or spinlock and then blocks until a condition
 * becomes true. The specified mutex/spinlock should be held by the calling
 * thread. When the function returns (upon both failure and success) the
 * mutex/spinlock will be held again by the calling thread. A condition becomes
 * true when either condvar_signal() or condvar_broadcast() is called on it.
 *
 * @note		You must not specify both a mutex and a spinlock. You
 *			can, however, specify neither.
 *
 * @param cv		Condition variable to wait on.
 * @param mtx		Mutex to unlock/relock.
 * @param sl		Spinlock to unlock/relock.
 * @param timeout	Timeout in microseconds. If 0 is specified, then the
 *			function will return an error immediately. If -1, it
 *			will block indefinitely until the condition becomes
 *			true.
 * @param flags		Synchronization flags.
 *
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set.
 */
status_t condvar_wait_etc(condvar_t *cv, mutex_t *mtx, spinlock_t *sl, useconds_t timeout, int flags) {
	status_t ret;
	bool state;

	assert(!(mtx && sl));

	/* Acquire the wait queue lock and disable interrupts, then release
	 * the specified lock. */
	state = waitq_sleep_prepare(&cv->queue);
	if(mtx) {
		mutex_unlock(mtx);
	} else if(sl) {
		assert(!state);
		spinlock_unlock_ni(sl);
	}

	/* Go to sleep. */
	ret = waitq_sleep_unsafe(&cv->queue, timeout, flags, state);

	/* Re-acquire the lock. */
	if(mtx) {
		mutex_lock(mtx);
	} else if(sl) {
		spinlock_lock_ni(sl);
	}

	return ret;
}

/**
 * Wait for a condition to become true.
 *
 * Atomically unlocks a mutex or spinlock and then blocks until a condition
 * becomes true. The specified mutex/spinlock should be held by the calling
 * thread. When the function returns the mutex/spinlock will be held again by
 * the calling thread. A condition becomes true when either condvar_signal()
 * or condvar_broadcast() is called on it.
 *
 * @param cv		Condition variable to wait on.
 * @param mtx		Mutex to unlock/relock.
 * @param sl		Spinlock to unlock/relock. Must not specify both mutex
 *			and spinlock.
 */
void condvar_wait(condvar_t *cv, mutex_t *mtx, spinlock_t *sl) {
	condvar_wait_etc(cv, mtx, sl, -1, 0);
}

/**
 * Signal that a condition has become true.
 *
 * Wakes the first thread (if any) waiting for a condition variable to become
 * true.
 *
 * @param cv		Condition variable to signal.
 *
 * @return		Whether a thread was woken.
 */
bool condvar_signal(condvar_t *cv) {
	return waitq_wake(&cv->queue);
}

/**
 * Broadcast that a condition has become true.
 *
 * Wakes all threads (if any) currently waiting for a condition variable to
 * become true.
 *
 * @param cv		Condition variable to broadcast.
 *
 * @return		Whether any threads were woken.
 */
bool condvar_broadcast(condvar_t *cv) {
	return waitq_wake_all(&cv->queue);
}

/** Initialise a condition variable.
 * @param cv		Condition variable to initialise.
 * @param name		Name to give the condition variable. */
void condvar_init(condvar_t *cv, const char *name) {
	waitq_init(&cv->queue, name);
}
