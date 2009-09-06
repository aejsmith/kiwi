/* Kiwi semaphore implementation
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
 * @brief		Semaphore implementation.
 */

#include <sync/semaphore.h>

#include <errors.h>

/** Down a semaphore.
 *
 * Attempts to down (decrease the value of) a semaphore. If SYNC_NONBLOCK is
 * specified, the function will return if it is unable to down, otherwise
 * it will block until it is able to perform the down.
 *
 * @param sem		Semaphore to down.
 * @param flags		Synchronization flags.
 *
 * @return		0 on success (always the case if neither SYNC_NONBLOCK
 *			or SYNC_INTERRUPTIBLE are specified), negative error
 *			code on failure.
 */
int semaphore_down(semaphore_t *sem, int flags) {
	return waitq_sleep(&sem->queue, NULL, NULL, flags);
}

/** Up a semaphore.
 *
 * Ups (increases the value of) a semaphore, and unblocks threads waiting if
 * necessary.
 *
 * @param sem		Semaphore to up.
 * @param count		Value to increment by.
 */
void semaphore_up(semaphore_t *sem, size_t count) {
	for(size_t i = 0; i < count; i++) {
		waitq_wake(&sem->queue, false);
	}
}

/** Initialise a semaphore structure.
 *
 * Initialises a semaphore structure and sets its initial count to the
 * value specified.
 *
 * @param sem		Semaphore to initialise.
 * @param name		Name of the semaphore, for debugging purposes.
 * @param initial	Initial value of the semaphore.
 *
 * @return		True if attempt to down succeeds, false otherwise.
 */
void semaphore_init(semaphore_t *sem, const char *name, unsigned int initial) {
	waitq_init(&sem->queue, name, WAITQ_COUNT_MISSED);
	sem->queue.missed = initial;
}
