/* Kiwi condition variable code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Condition variable code.
 */

#include <sync/condvar.h>

#include <assert.h>

/** Wait for a condition to become true.
 *
 * Atomically unlocks a mutex or spinlock and then blocks until a condition
 * becomes true. The specified mutex/spinlock should be held by the calling
 * thread. When the function returns (upon both failure and success) the
 * mutex/spinlock will be held again by the calling thread. A condition becomes
 * true when either condvar_signal() or condvar_broadcast() is called on it. It
 * is pointless to specify the SYNC_NONBLOCK flag - the call will always return
 * an error if it is set.
 *
 * @param cv		Condition variable to wait on.
 * @param mtx		Mutex to unlock/relock.
 * @param sl		Spinlock to unlock/relock. Must not specify both mutex
 *			and spinlock.
 * @param flags		Synchronization flags.
 *
 * @return		0 on success (always the case if neither SYNC_NONBLOCK
 *			or SYNC_INTERRUPTIBLE are specified), negative error
 *			code on failure.
 */
int condvar_wait(condvar_t *cv, mutex_t *mtx, spinlock_t *sl, int flags) {
	assert(!!mtx ^ !!sl);
	return waitq_sleep(&cv->queue, mtx, sl, flags);
}

/** Signal that a condition has become true.
 *
 * Wakes the first thread (if any) waiting for a condition variable to become
 * true.
 *
 * @param cv		Condition variable to signal.
 *
 * @return		Whether a thread was woken.
 */
bool condvar_signal(condvar_t *cv) {
	return waitq_wake(&cv->queue, false);
}

/** Broadcast that a condition has become true.
 *
 * Wakes all threads (if any) currently waiting for a condition variable to
 * become true.
 *
 * @param cv		Condition variable to broadcast.
 *
 * @return		Whether any threads were woken.
 */
bool condvar_broadcast(condvar_t *cv) {
	return waitq_wake(&cv->queue, true);
}

/** Initialize a condition variable.
 *
 * Initializes the given condition variable structure.
 *
 * @param cv		Condition variable to initialize.
 * @param name		Name to give the condition variable.
 */
void condvar_init(condvar_t *cv, const char *name) {
	waitq_init(&cv->queue, name, 0);
}
