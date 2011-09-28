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
 * @brief		Readers-writer lock implementation.
 *
 * Ideas for this implementation, particularly on how to prevent thread
 * starvation, are from HelenOS' readers-writer lock implementation.
 */

#include <proc/thread.h>
#include <sync/rwlock.h>
#include <status.h>

extern void thread_wake(thread_t *thread);

/** Transfer lock ownership to a waiting writer or waiting readers.
 * @note		Queue lock should be held.
 * @param lock		Lock to transfer ownership of. */
static void rwlock_transfer_ownership(rwlock_t *lock) {
	thread_t *thread;

	/* Check if there are any threads to transfer ownership to. */
	if(list_empty(&lock->queue.threads)) {
		/* There aren't. If there are still readers (it is possible for
		 * there to be, because this function gets called if a writer
		 * is interrupted while blocking in order to allow readers
		 * queued behind it in), then we do not want to do anything.
		 * Otherwise, release the lock. */
		if(!lock->readers) {
			lock->held = 0;
		}
		return;
	}

	/* Go through all threads queued. */
	LIST_FOREACH_SAFE(&lock->queue.threads, iter) {
		thread = list_entry(iter, thread_t, waitq_link);

		spinlock_lock(&thread->lock);

		/* If it is a reader, we can wake it and continue. If it is a
		 * writer and the lock has no readers, wake it up and finish.
		 * If it is a writer and the lock has readers, finish. */
		if(thread->rwlock_writer && lock->readers) {
			spinlock_unlock(&thread->lock);
			break;
		} else {
			thread_wake(thread);
			if(thread->rwlock_writer) {
				spinlock_unlock(&thread->lock);
				break;
			} else {
				/* We must increment the reader count here. */
				lock->readers++;
				spinlock_unlock(&thread->lock);
			}
		}
	}
}

/**
 * Acquire a readers-writer lock for reading.
 *
 * Acquires a readers-writer lock for reading. Multiple readers can hold a
 * readers-writer lock at any one time, however if there are any writers
 * waiting for the lock, the function will block and allow the writer to take
 * the lock, in order to prevent starvation of writers.
 *
 * @param lock		Lock to acquire.
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
status_t rwlock_read_lock_etc(rwlock_t *lock, useconds_t timeout, int flags) {
	bool state;

	state = waitq_sleep_prepare(&lock->queue);

	if(lock->held) {
		/* Lock is held, check if it's held by readers. If it is, and
		 * there's something waiting on the queue, we wait anyway. This
		 * is to prevent starvation of writers. */
		if(!lock->readers || !list_empty(&lock->queue.threads)) {
			/* Readers count will have been incremented for us
			 * upon success. */
			curr_thread->rwlock_writer = false;
			return waitq_sleep_unsafe(&lock->queue, timeout, flags, state);
		}
	} else {
		lock->held = 1;
	}

	lock->readers++;
	waitq_sleep_cancel(&lock->queue, state);
	return STATUS_SUCCESS;
}

/**
 * Acquire a readers-writer lock for writing.
 *
 * Acquires a readers-writer lock for writing. When the lock has been acquired,
 * no other readers or writers will be holding the lock, or be able to acquire
 * it.
 *
 * @param lock		Lock to acquire.
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
status_t rwlock_write_lock_etc(rwlock_t *lock, useconds_t timeout, int flags) {
	status_t ret = STATUS_SUCCESS;
	bool state;

	state = waitq_sleep_prepare(&lock->queue);

	/* Just acquire the exclusive lock. */
	if(lock->held) {
		curr_thread->rwlock_writer = true;

		ret = waitq_sleep_unsafe(&lock->queue, timeout, flags, state);
		if(ret != STATUS_SUCCESS) {
			/* Failed to acquire the lock. In this case, there may
			 * be a reader queued behind us that can be let in. */
			spinlock_lock(&lock->queue.lock);
			if(lock->readers) {
				rwlock_transfer_ownership(lock);
			}
			spinlock_unlock(&lock->queue.lock);
		}
	} else {
		lock->held = 1;
		waitq_sleep_cancel(&lock->queue, state);
	}

	return ret;
}

/**
 * Acquire a readers-writer lock for reading.
 *
 * Acquires a readers-writer lock for reading. Multiple readers can hold a
 * readers-writer lock at any one time, however if there are any writers
 * waiting for the lock, the function will block and allow the writer to take
 * the lock, in order to prevent starvation of writers.
 *
 * @param lock		Lock to acquire.
 */
void rwlock_read_lock(rwlock_t *lock) {
	rwlock_read_lock_etc(lock, -1, 0);
}

/**
 * Acquire a readers-writer lock for writing.
 *
 * Acquires a readers-writer lock for writing. When the lock has been acquired,
 * no other readers or writers will be holding the lock, or be able to acquire
 * it.
 *
 * @param lock		Lock to acquire.
 */
void rwlock_write_lock(rwlock_t *lock) {
	rwlock_write_lock_etc(lock, -1, 0);
}

/** Release a readers-writer lock.
 * @param lock		Lock to release. */
void rwlock_unlock(rwlock_t *lock) {
	spinlock_lock(&lock->queue.lock);

	if(!lock->held) {
		fatal("Unlock of unheld rwlock %p(%s)", lock, lock->queue.name);
	} else if(!lock->readers || !--lock->readers) {
		rwlock_transfer_ownership(lock);
	}

	spinlock_unlock(&lock->queue.lock);
}

/** Initialise a readers-writer lock.
 * @param lock		Lock to initialise.
 * @param name		Name to give lock. */
void rwlock_init(rwlock_t *lock, const char *name) {
	waitq_init(&lock->queue, name);
	lock->held = 0;
	lock->readers = 0;
}
