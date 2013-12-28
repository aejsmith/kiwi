/*
 * Copyright (C) 2008-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Semaphore implementation.
 */

#include <proc/process.h>

#include <sync/semaphore.h>

#include <kernel.h>
#include <status.h>

/** Down a semaphore.
 * @param sem		Semaphore to down.
 * @param timeout	Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *			will always be taken to be a system time at which the
 *			sleep will time out. Otherwise, taken as the number of
 *			nanoseconds in which the sleep will time out. If 0 is
 *			specified, the function will return an error immediately
 *			if the lock cannot be acquired immediately. If -1
 *			is specified, the thread will sleep indefinitely until
 *			the semaphore can be downed or it is interrupted.
 * @param flags		Sleeping behaviour flags.
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SLEEP_INTERRUPTIBLE flag is set. */
status_t semaphore_down_etc(semaphore_t *sem, nstime_t timeout, unsigned flags) {
	spinlock_lock(&sem->lock);

	if(sem->count) {
		--sem->count;
		spinlock_unlock(&sem->lock);
		return STATUS_SUCCESS;
	}

	list_append(&sem->threads, &curr_thread->wait_link);
	return thread_sleep(&sem->lock, timeout, sem->name, flags);
}

/** Down a semaphore.
 * @param sem		Semaphore to down. */
void semaphore_down(semaphore_t *sem) {
	semaphore_down_etc(sem, -1, 0);
}

/** Up a semaphore.
 * @param sem		Semaphore to up.
 * @param count		Value to increment the count by. */
void semaphore_up(semaphore_t *sem, size_t count) {
	thread_t *thread;
	size_t i;

	spinlock_lock(&sem->lock);

	for(i = 0; i < count; i++) {
		if(list_empty(&sem->threads)) {
			sem->count++;
		} else {
			thread = list_first(&sem->threads, thread_t, wait_link);
			thread_wake(thread);
		}
	}

	spinlock_unlock(&sem->lock);
}

/**
 * Reset a semaphore.
 *
 * Wakes all threads currently waiting on a semaphore and resets the semaphore
 * count to the specified value. The semaphore_down() calls will not return an
 * error - code that uses this function must be able to handle being unblocked
 * like this.
 *
 * @param sem		Semaphore to reset.
 * @param initial	Initial count value.
 */
void semaphore_reset(semaphore_t *sem, size_t initial) {
	thread_t *thread;

	spinlock_lock(&sem->lock);

	while(!list_empty(&sem->threads)) {
		thread = list_first(&sem->threads, thread_t, wait_link);
		thread_wake(thread);
	}

	sem->count = initial;

	spinlock_unlock(&sem->lock);
}

/** Initialize a semaphore structure.
 * @param sem		Semaphore to initialize.
 * @param name		Name of the semaphore, for debugging purposes.
 * @param initial	Initial value of the semaphore. */
void semaphore_init(semaphore_t *sem, const char *name, size_t initial) {
	spinlock_init(&sem->lock, "semaphore_lock");
	list_init(&sem->threads);
	sem->count = initial;
	sem->name = name;
}
