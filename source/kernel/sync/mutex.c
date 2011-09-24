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
 * @brief		Mutex implementation.
 */

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <status.h>
#include <symbol.h>

/** Handle a recursive locking error.
 * @param lock		Lock error occurred on. */
static inline void mutex_recursive_error(mutex_t *lock) {
#if CONFIG_DEBUG
	size_t off = 0;
	symbol_t *sym;

	sym = symbol_lookup_addr((ptr_t)lock->caller, &off);
	fatal("Recursive locking of non-recursive mutex %p(%s)\n"
	      "Locked by [%p] %s+0x%zx", lock, lock->queue.name, lock->caller,
	      (sym) ? sym->name : "<unknown>", off);
#else
	fatal("Recursive locking of non-recursive mutex %p(%s)",
	      lock, lock->queue.name);
#endif
}

/** Internal mutex locking code.
 * @param lock		Mutex to lock.
 * @param timeout	Timeout in microseconds.
 * @param flags		Synchronization flags.
 * @return		Status code describing result of the operation. */
static inline status_t mutex_lock_internal(mutex_t *lock, useconds_t timeout, int flags) {
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
				mutex_recursive_error(lock);
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

	ret = mutex_lock_internal(lock, timeout, flags);
#if CONFIG_DEBUG
	if(likely(ret == STATUS_SUCCESS)) {
		lock->caller = __builtin_return_address(0);
	}
#endif
	return ret;
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
#if CONFIG_DEBUG
	status_t ret;

	ret = mutex_lock_internal(lock, -1, 0);
	assert(ret == STATUS_SUCCESS);
	lock->caller = __builtin_return_address(0);
#else
	mutex_lock_internal(lock, -1, 0);
#endif
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
