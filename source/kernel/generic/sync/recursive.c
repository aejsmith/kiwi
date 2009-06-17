/* Kiwi recursive lock implementation
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
 * @brief		Recursive lock implementation.
 */

#include <sync/recursive.h>

#include <proc/thread.h>

#include <fatal.h>

/** Acquire a recursive lock.
 *
 * Attempts to acquire a recursive lock. If SYNC_NONBLOCK is specified, the
 * function will return if it is unable to take the lock immediately, otherwise
 * it will block until it is able to do so. If the calling thread already
 * holds the lock, then its recursion count will be increased and the function
 * will return immediately.
 *
 * @param lock		Recursive lock to acquire.
 * @param flags		Synchronization flags.
 *
 * @return		0 if succeeded (always the case if SYNC_NONBLOCK is
 *			not specified), negative error code on failure.
 */
int recursive_lock_acquire(recursive_lock_t *lock, int flags) {
	int ret;

	if(lock->holder != curr_thread) {
		ret = semaphore_down(&lock->sem, flags);
		if(ret != 0) {
			return ret;
		}

		lock->holder = curr_thread;
	}

	lock->recursion++;
	return 0;
}

/** Release a recursive lock.
 *
 * Releases a recursive lock. If the recursion count is non-zero after
 * decreasing, then the lock will remain held by the current thread.
 *
 * @param lock		Recursive lock to release.
 */
void recursive_lock_release(recursive_lock_t *lock) {
	if(!lock->recursion) {
		fatal("Release of unheld recursive lock 0x%p(%s)", lock, lock->sem.queue.name);
	} else if(lock->holder != curr_thread) {
		fatal("Release of recursive lock 0x%p(%s) from incorrect thread\n"
		      "Holder: 0x%p  Current: 0x%p",
		      lock, lock->sem.queue.name, lock->holder, curr_thread);
	}

	/* Check holder: use of this function before the scheduler is up
	 * could mean that the holder is NULL. */
	if(--lock->recursion == 0 && lock->holder) {
		lock->holder = NULL;
		semaphore_up(&lock->sem);
	}
}

/** Initialize a recursive lock.
 *
 * Initializes a recursive lock structure.
 *
 * @param lock		Recursive lock to initialize.
 * @param name		Name of the lock (for debugging purposes).
 */
void recursive_lock_init(recursive_lock_t *lock, const char *name) {
	semaphore_init(&lock->sem, name, 1);
	lock->recursion = 0;
	lock->holder = NULL;
}
