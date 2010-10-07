/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Mutex implementation.
 */

#include <cpu/intr.h>

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <fatal.h>
#include <status.h>

/** Lock a mutex.
 *
 * Attempts to lock a mutex. If the mutex is recursive, and the calling thread
 * already holds the lock, then its recursion count will be increased and the
 * function will return immediately.
 *
 * @param lock		Mutex to lock.
 * @param timeout	Timeout in microseconds. A timeout of -1 will sleep
 *			forever until the lock is acquired, and a timeout of 0
 *			will return an error immediately if unable to acquire
 *			the lock.
 * @param flags		Synchronization flags.
 *
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set.
 */
status_t mutex_lock_etc(mutex_t *lock, useconds_t timeout, int flags) {
	status_t ret;
	bool state;

	/* Try to take the lock. */
	if(!atomic_cmp_set(&lock->locked, 0, 1)) {
		if(lock->holder == curr_thread) {
			/* Wrap this with likely because if held by the current
			 * thread the MUTEX_RECURSIVE flag should be set. */
			if(likely(lock->flags & MUTEX_RECURSIVE)) {
				atomic_inc(&lock->locked);
				return STATUS_SUCCESS;
			} else {
				fatal("Recursive locking of non-recursive mutex %p(%s)",
				      lock, lock->queue.name);
			}
                } else {
			state = waitq_sleep_prepare(&lock->queue);

			/* Check again now that we have the wait queue lock,
			 * in case mutex_unlock() was called on another CPU. */
			if(atomic_cmp_set(&lock->locked, 0, 1)) {
				waitq_sleep_cancel(&lock->queue, state);
			} else {
				/* If sleep is successful, lock ownership will
				 * have been transferred to us. */
				ret = waitq_sleep_unsafe(&lock->queue, timeout, flags, state);
				if(ret != STATUS_SUCCESS) {
					return ret;
				}
			}
		}
	}

	lock->holder = curr_thread;
	return STATUS_SUCCESS;
}

/** Lock a mutex.
 *
 * Attempts to lock a mutex. If the mutex is recursive, and the calling thread
 * already holds the lock, then its recursion count will be increased and the
 * function will return immediately.
 *
 * @param lock		Mutex to lock.
 */
void mutex_lock(mutex_t *lock) {
	mutex_lock_etc(lock, -1, 0);
}

/** Unlock a mutex.
 *
 * Unlocks a mutex. Must be held by the current thread else a fatal error
 * will occur. It is also invalid to unlock an already unlocked mutex. If
 * the mutex is recursive, and the recursion count is greater than 1 at the
 * time this function is called, then the mutex will remain held.
 *
 * @param lock		Mutex to unlock.
 */
void mutex_unlock(mutex_t *lock) {
	spinlock_lock(&lock->queue.lock);

	if(unlikely(!atomic_get(&lock->locked))) {
		fatal("Unlock of unheld mutex %p(%s)", lock, lock->queue.name);
	} else if(unlikely(lock->holder != curr_thread)) {
		fatal("Unlock of mutex %p(%s) from incorrect thread (holder: %" PRIu32 ")",
		      lock, lock->queue.name, (lock->holder) ? lock->holder->id : -1);
	}

	/* If the current value is 1, the lock is being released. If there is
	 * a thread waiting, we do not need to modify the count, as we transfer
	 * ownership of the lock to it. Otherwise, decrement the count. */
	if(atomic_get(&lock->locked) == 1) {
		lock->holder = NULL;
		if(!waitq_wake_unsafe(&lock->queue)) {
			atomic_dec(&lock->locked);
		}
	} else {
		atomic_dec(&lock->locked);
	}

	spinlock_unlock(&lock->queue.lock);
}

/** Initialise a mutex.
 * @param lock		Mutex to initialise.
 * @param name		Name to give the mutex.
 * @param flags		Behaviour flags for the mutex. */
void mutex_init(mutex_t *lock, const char *name, int flags) {
	atomic_set(&lock->locked, 0);
	waitq_init(&lock->queue, name);
	lock->holder = NULL;
	lock->flags = flags;
}
