/* Kiwi mutex implementation
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
 * @brief		Mutex implementation.
 */

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>

/** Lock a mutex.
 *
 * Attempts to lock a mutex. If SYNC_NONBLOCK is specified, the function will
 * return if it is unable to take the lock immediately, otherwise it will block
 * until it is able to do so. If the mutex is recursive, and the calling thread
 * already holds the lock, then its recursion count will be increased and the
 * function will return immediately.
 *
 * @param lock		Mutex to lock.
 * @param flags		Synchronization flags.
 *
 * @return		0 on success (always the case if neither SYNC_NONBLOCK
 *			or SYNC_INTERRUPTIBLE are specified), negative error
 *			code on failure.
 */
int mutex_lock(mutex_t *lock, int flags) {
	int ret;

	if(lock->holder != curr_thread) {
		ret = semaphore_down(&lock->sem, flags);
		if(ret != 0) {
			return ret;
		}

		assert(!lock->recursion);
		lock->holder = curr_thread;
		lock->caller = (ptr_t)__builtin_return_address(0);
	} else if(curr_thread && !(lock->flags & MUTEX_RECURSIVE)) {
		fatal("Nested locking of mutex %p(%s) by %" PRIu32 "(%s)",
		      lock, lock->sem.queue.name, lock->holder->id,
		      lock->holder->name);
	}

	lock->recursion++;
	return 0;
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
	if(!lock->recursion) {
		fatal("Unlock of unheld mutex %p(%s)", lock, lock->sem.queue.name);
	} else if(lock->holder != curr_thread) {
		fatal("Unlock of mutex %p(%s) from incorrect thread\n"
		      "Holder: %p  Current: %p",
		      lock, lock->sem.queue.name, lock->holder, curr_thread);
	}

	assert(lock->recursion <= 1 || lock->flags & MUTEX_RECURSIVE);

	/* Check that holder is NULL because mutexes can be used when the
	 * scheduler is not up. In this case, mutex_lock() does not down the
	 * semaphore. */
	if(--lock->recursion == 0 && lock->holder) {
		lock->caller = 0;
		lock->holder = NULL;
		semaphore_up(&lock->sem, 1);
	}
}

/** Initialize a mutex.
 *
 * Initializes the given mutex structure.
 *
 * @param lock		Mutex to initialize.
 * @param name		Name to give the mutex.
 * @param flags		Behaviour flags for the mutex.
 */
void mutex_init(mutex_t *lock, const char *name, int flags) {
	semaphore_init(&lock->sem, name, 1);
	lock->flags = flags;
	lock->holder = NULL;
	lock->recursion = 0;
}
