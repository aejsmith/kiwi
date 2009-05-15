/* Kiwi wait queue functions
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
 * @brief		Wait queue functions.
 */

#include <cpu/intr.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/waitq.h>

#include <assert.h>
#include <errors.h>

extern void sched_internal(bool state);
extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);

/** Sleep on a wait queue.
 *
 * Inserts the current thread into the specified wait queue and then sleeps
 * until it is woken by wait_queue_wake(). If the wait queue was created with
 * WAITQ_COUNT_MISSED, then the SYNC_NONBLOCK flag will cause the function to
 * return false if there is not a missed wakeup available. Otherwise, it will
 * have no effect.
 *
 * @param wait		Wait queue to wait on.
 * @param flags		Synchronization flags (see sync/flags.h)
 *
 * @return		0 on success, negative error code on failure.
 */
int wait_queue_sleep(wait_queue_t *waitq, int flags) {
	bool state = intr_disable();

	spinlock_lock_ni(&waitq->lock, 0);

	if(waitq->flags & WAITQ_COUNT_MISSED) {
		if(waitq->missed > 0) {
			waitq->missed--;

			spinlock_unlock_ni(&waitq->lock);
			intr_restore(state);
			return 0;
		} else if(flags & SYNC_NONBLOCK) {
			spinlock_unlock_ni(&waitq->lock);
			intr_restore(state);
			return -ERR_WOULD_BLOCK;
		}
	}

	spinlock_lock_ni(&curr_thread->lock, 0);

	curr_thread->waitq = waitq;

	/* Set up interruption context if required. OK for this to be done
	 * with thread locked: restoring this context will be performed by
	 * the thread switch code, and the thread will be locked when it is
	 * restored. */
	if(flags & SYNC_INTERRUPTIBLE) {
		curr_thread->interruptible = true;

		if(context_save(&curr_thread->sleep_context) != 0) {
			sched_post_switch(state);
			return -ERR_INTERRUPTED;
		}
	} else {
		curr_thread->interruptible = false;
	}

	/* Add the thread to the queue and unlock it. */
	list_append(&waitq->threads, &curr_thread->waitq_link);
	spinlock_unlock_ni(&waitq->lock);

	/* Send the thread to sleep. The scheduler will handle interrupt state
	 * and thread locking. */
	curr_thread->state = THREAD_SLEEPING;
	sched_internal(state);
	return 0;
}

/** Wake up next thread on a wait queue.
 *
 * Wakes up the next thread in a wait queue.
 *
 * @param waitq		Wait queue to wake from.
 *
 * @return		True if a thread was woken, false if queue was empty.
 */
bool wait_queue_wake(wait_queue_t *waitq) {
	thread_t *thread;

	spinlock_lock(&waitq->lock, 0);

	if(!list_empty(&waitq->threads)) {
		thread = list_entry(waitq->threads.next, thread_t, waitq_link);

		spinlock_lock(&thread->lock, 0);

		assert(thread->state == THREAD_SLEEPING);

		thread->state = THREAD_READY;
		sched_thread_insert(thread);

		thread->waitq = NULL;
		thread->interruptible = false;

		spinlock_unlock(&thread->lock);
		spinlock_unlock(&waitq->lock);
		return true;
	} else {
		if(waitq->flags & WAITQ_COUNT_MISSED) {
			waitq->missed++;
		}

		spinlock_unlock(&waitq->lock);
		return false;
	}
}

/** Interrupt a sleeping thread.
 *
 * Interrupts a thread that is sleeping on a wait queue if possible.
 *
 * @param thread	Thread to interrupt.
 */
void wait_queue_interrupt(thread_t *thread) {
	spinlock_lock(&thread->lock, 0);

	assert(thread->state == THREAD_SLEEPING);
	assert(thread->waitq);

	if(thread->interruptible) {
		spinlock_lock(&thread->waitq->lock, 0);
		list_remove(&thread->waitq_link);
		spinlock_unlock(&thread->waitq->lock);

		/* Restore the interruption context. */
		thread->context = thread->sleep_context;

		thread->state = THREAD_READY;
		sched_thread_insert(thread);

		thread->waitq = NULL;
		thread->interruptible = false;
	}

	spinlock_unlock(&thread->lock);
}

/** Check if a wait queue is empty.
 *
 * Checks if a wait queue is empty.
 *
 * @param waitq		Wait queue to check.
 *
 * @return		True if empty, false if not.
 */
bool wait_queue_empty(wait_queue_t *waitq) {
	bool ret;

	spinlock_lock(&waitq->lock, 0);
	ret = list_empty(&waitq->threads);
	spinlock_unlock(&waitq->lock);

	return ret;
}

/** Initialize a wait queue.
 *
 * Initializes the specified wait queue structure.
 *
 * @param waitq		Wait queue to initialize.
 * @param name		Name of wait queue.
 * @param flags		Flags for the queue.
 */
void wait_queue_init(wait_queue_t *waitq, const char *name, int flags) {
	spinlock_init(&waitq->lock, "wait_queue_lock");
	list_init(&waitq->threads);

	waitq->flags = flags;
	waitq->missed = 0;
	waitq->name = name;
}
