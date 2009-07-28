/* Kiwi readers-writer lock implementation
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
 * @brief		Readers-writer lock implementation.
 *
 * Ideas for this implementation, particularly on how to prevent thread
 * starvation, are from HelenOS' readers-writer lock implementation.
 */

#include <proc/thread.h>

#include <sync/rwlock.h>

extern void wait_queue_do_wake(thread_t *thread);

/** Transfer lock ownership to waiting writer or waiting readers.
 * @note		Spinlock should be held.
 * @param lock		Lock to transfer ownership of. */
static void rwlock_transfer_ownership(rwlock_t *lock) {
	wait_queue_t *queue;
	thread_t *thread;

	/* Meh, take a copy of it just to make the code a bit nicer. */
	queue = &lock->exclusive.queue;

	spinlock_lock(&queue->lock, 0);

	/* Check if there are any threads to transfer ownership to. */
	if(list_empty(&queue->threads)) {
		/* There aren't. If there are still readers (it is possible for
		 * there to be, because this function gets called if a writer
		 * is interrupted while blocking in order to allow readers
		 * queued behind it in), then we do not want to do anything.
		 * Otherwise, release the lock. */
		if(!lock->readers) {
			queue->missed++;
		}
		spinlock_unlock(&queue->lock);
		return;
	}

	/* Go through all threads queued. */
	LIST_FOREACH_SAFE(&queue->threads, iter) {
		thread = list_entry(iter, thread_t, waitq_link);

		spinlock_lock(&thread->lock, 0);

		/* If it is a reader, we can wake it and continue. If it is a
		 * writer and the lock has no readers, wake it up and finish.
		 * If it is a writer and the lock has readers, finish. */
		if(thread->rwlock_writer && lock->readers) {
			spinlock_unlock(&thread->lock);
			break;
		} else {
			wait_queue_do_wake(thread);
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

	spinlock_unlock(&queue->lock);
}

/** Acquire a readers-writer lock for reading.
 *
 * Acquires a readers-writer lock for reading. If the lock is currently held
 * by any other readers, the call will succeed immediately. If it is not held
 * at all, then it will succeed. Otherwise, the lock must be held by a writer,
 * and the call will not succeed until the writer releases the lock (or, if
 * SYNC_NONBLOCK is specified, an error will be returned immediately).
 *
 * @param lock		Lock to acquire.
 * @param flags		Synchronization flags.
 *
 * @return		0 on success (always the case if SYNC_NONBLOCK is not
 *			specified), negative error code on failure.
 */
int rwlock_read_lock(rwlock_t *lock, int flags) {
	int ret;

	curr_thread->rwlock_writer = false;

	spinlock_lock(&lock->lock, 0);

	/* If we can take the exclusive lock without blocking, we're OK. */
	if((ret = semaphore_down(&lock->exclusive, SYNC_NONBLOCK)) != 0) {
		/* Lock is held, check if its held by readers. If it is, and
		 * there's something else blocked on the lock, we wait anyway.
		 * This is to prevent starvation of writers. */
		if(!lock->readers || !wait_queue_empty(&lock->exclusive.queue)) {
			spinlock_unlock(&lock->lock);
			if((ret = semaphore_down(&lock->exclusive, flags)) != 0) {
				return ret;
			}

			/* Readers count will have been incremented for us. */
			return 0;
		}
	}

	lock->readers++;
	spinlock_unlock(&lock->lock);
	return 0;
}

/** Acquire a readers-writer lock for writing.
 *
 * Acquires a readers-writer lock for writing. When the lock has been acquired,
 * no other readers or writers will be holding the lock, or be able to acquire
 * it. If SYNC_NONBLOCK is specified, an error will be returned if the call is
 * not able to acquire the lock immediately.
 *
 * @param lock		Lock to acquire.
 * @param flags		Synchronization flags.
 *
 * @return		0 on success (always the case if SYNC_NONBLOCK is not
 *			specified), negative error code on failure.
 */
int rwlock_write_lock(rwlock_t *lock, int flags) {
	int ret;

	curr_thread->rwlock_writer = true;

	/* Just acquire the exclusive lock. */
	if((ret = semaphore_down(&lock->exclusive, flags)) != 0) {
		/* Failed to acquire the lock, we may have been interrupted.
		 * In this case, there may be a reader queued behind us that
		 * can be let in. */
		spinlock_lock(&lock->lock, 0);
		if(lock->readers) {
			rwlock_transfer_ownership(lock);
		}
		spinlock_unlock(&lock->lock);
	}

	return ret;
}

/** Release a readers-writer lock.
 *
 * Releases a readers-writer lock. 
 *
 * @param lock		Lock to release.
 */
void rwlock_unlock(rwlock_t *lock) {
	spinlock_lock(&lock->lock, 0);

	if(lock->readers && --lock->readers) {
		spinlock_unlock(&lock->lock);
		return;
	}

	rwlock_transfer_ownership(lock);
	spinlock_unlock(&lock->lock);
}

/** Initialize a readers-writer lock.
 *
 * Initializes a readers-writer lock structure.
 *
 * @param lock		Lock to initialize.
 * @param name		Name to give lock.
 */
void rwlock_init(rwlock_t *lock, const char *name) {
	/* Name the spinlock "rwlock_lock" because that lock is for use
	 * internally, so if any locking bugs occur internally, it'll be more
	 * obvious where it has happened. The exclusive semaphore is given the
	 * name we're provided so that it will show up as the wait queue name
	 * if a thread is blocking on it. */
	spinlock_init(&lock->lock, "rwlock_lock");
	semaphore_init(&lock->exclusive, name, 1);
	lock->readers = 0;
}
