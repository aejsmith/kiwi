/*
 * Copyright (C) 2010 Alex Smith
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
 *
 * This implementation is based around the "Mutex, take 3" implementation in
 * the paper linked below. The futex has 3 states:
 *  - 0 - Unlocked.
 *  - 1 - Locked, no waiters.
 *  - 2 - Locked, one or more waiters.
 *
 * Reference:
 *  - Futexes are Tricky
 *    http://dept-info.labri.fr/~denis/Enseignement/2008-IR/Articles/01-futex.pdf
 *
 * @todo		Make this fair.
 */

#include <kernel/futex.h>
#include <kernel/status.h>

#include <util/mutex.h>

/** Check whether a mutex is held.
 * @param lock		Lock to check.
 * @return		Whether the lock is held. */
bool libc_mutex_held(libc_mutex_t *lock) {
	return (lock->futex != 0);
}

/** Acquire a mutex.
 * @param lock		Lock to acquire.
 * @param timeout	Timeout in microseconds. If -1, the function will block
 *			indefinitely until able to acquire the mutex. If 0, an
 *			error will be returned if the lock cannot be acquired
 *			immediately.
 * @return		Status code describing result of the operation. */
status_t libc_mutex_lock(libc_mutex_t *lock, useconds_t timeout) {
	status_t ret;
	int32_t val;

	/* If the futex is currently 0 (unlocked), just set it to 1 (locked, no
	 * waiters) and return. */
	val = __sync_val_compare_and_swap(&lock->futex, 0, 1);
	if(val != 0) {
		if(timeout == 0) {
			return STATUS_TIMED_OUT;
		}

		/* Set futex to 2 (locked with waiters). */
		if(val != 2) {
			val = __sync_lock_test_and_set(&lock->futex, 2);
		}

		/* Loop until we can acquire the futex. */
		while(val != 0) {
			ret = kern_futex_wait((int32_t *)&lock->futex, 2, timeout);
			if(ret != STATUS_SUCCESS && ret != STATUS_TRY_AGAIN) {
				return ret;
			}

			/* We cannot know whether there are waiters or not.
			 * Therefore, to be on the safe side, set that there
			 * are (see paper linked above). */
			val = __sync_lock_test_and_set(&lock->futex, 2);
		}
	}

	return STATUS_SUCCESS;
}

/** Release a mutex.
 * @param lock		Lock to release. */
void libc_mutex_unlock(libc_mutex_t *lock) {
	if(__sync_fetch_and_sub(&lock->futex, 1) != 1) {
		/* There were waiters. Wake one up. */
		lock->futex = 0;
		kern_futex_wake((int32_t *)&lock->futex, 1, NULL);
	}
}

/** Initialise a mutex.
 * @param lock		Lock to initialise. */
void libc_mutex_init(libc_mutex_t *lock) {
	lock->futex = 0;
}
