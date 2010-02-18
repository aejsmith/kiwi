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
 * @brief		Semaphore implementation.
 */

#include <cpu/intr.h>
#include <sync/semaphore.h>

/** Down a semaphore.
 * @param sem		Semaphore to down.
 * @param timeout	Timeout in microseconds. A timeout of -1 will sleep
 *			forever until the lock is acquired, and a timeout of 0
 *			will return an error immediately if unable to down
 *			the semaphore.
 * @param flags		Synchronization flags.
 * @return		0 on success, negative error code on failure. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set. */
int semaphore_down_etc(semaphore_t *sem, timeout_t timeout, int flags) {
	bool state;

	state = waitq_sleep_prepare(&sem->queue);
	if(sem->count) {
		--sem->count;
		spinlock_unlock_ni(&sem->queue.lock);
		intr_restore(state);
		return 0;
	}

	return waitq_sleep_unsafe(&sem->queue, timeout, flags, state);
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
	size_t i;

	spinlock_lock(&sem->queue.lock);
	for(i = 0; i < count; i++) {
		if(!waitq_wake_unsafe(&sem->queue)) {
			sem->count++;
		}
	}
	spinlock_unlock(&sem->queue.lock);
}

/** Initialise a semaphore structure.
 * @param sem		Semaphore to initialise.
 * @param name		Name of the semaphore, for debugging purposes.
 * @param initial	Initial value of the semaphore. */
void semaphore_init(semaphore_t *sem, const char *name, size_t initial) {
	waitq_init(&sem->queue, name);
	sem->count = initial;
}
