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

#include <errors.h>
#include <fatal.h>

/** Internal part of mutex_lock().
 * @param mutex		Mutex to lock.
 * @return		True if locked, false if not. */
static inline bool mutex_try_lock(mutex_t *mutex) {
	if(!atomic_cmp_set(&mutex->locked, 0, 1)) {
		if(mutex->holder == curr_thread) {
			fatal("Nested locking of mutex 0x%p(%s) by %" PRIu32 "(%s)",
			      mutex, mutex->queue.name, mutex->holder->id,
			      mutex->holder->name);
		}
		return false;
	}

	mutex->holder = curr_thread;
	return true;
}

/** Lock a mutex.
 *
 * Attempts to lock a mutex. If SYNC_NONBLOCK is specified, the function will
 * return if it is unable to take the lock immediately, otherwise it will block
 * until it is able to do so.
 *
 * @param mutex		Mutex to lock.
 * @param flags		Synchronization flags.
 *
 * @return		0 if succeeded (always the case if SYNC_NONBLOCK is
 *			not specified), negative error code on failure.
 */
int mutex_lock(mutex_t *mutex, int flags) {
	int ret;

	while(!mutex_try_lock(mutex)) {
		if(flags & SYNC_NONBLOCK) {
			return -ERR_WOULD_BLOCK;
		}

		ret = wait_queue_sleep(&mutex->queue, flags);
		if(ret != 0) {
			return ret;
		}
	}

	return 0;
}

/** Unlock a mutex.
 *
 * Unlocks a mutex. Must be held by the current thread else a fatal error
 * will occur. It is also invalid to unlock an already unlocked mutex.
 *
 * @todo		Transfer lock ownership to a waiting thread if there
 *			is one.
 *
 * @param mutex		Mutex to unlock.
 */
void mutex_unlock(mutex_t *mutex) {
	if(atomic_get(&mutex->locked) == 0) {
		fatal("Attempted unlock of unlocked mutex 0x%p(%s)", mutex, mutex->queue.name);
	} else if(mutex->holder != curr_thread) {
		fatal("Attempted unlock of mutex 0x%p(%s) from incorrect thread", mutex, mutex->queue.name);
	}

	mutex->holder = NULL;
	atomic_set(&mutex->locked, 0);
	wait_queue_wake(&mutex->queue);
}

/** Initialize a mutex.
 *
 * Initializes the given mutex structure.
 *
 * @param mutex		Mutex to initialize.
 * @param name		Name to give the mutex.
 */
void mutex_init(mutex_t *mutex, const char *name) {
	wait_queue_init(&mutex->queue, name, 0);
	atomic_set(&mutex->locked, 0);

	mutex->holder = NULL;
}
