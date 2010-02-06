/*
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

#include <sync/mutex.h>
#include <sync/waitq.h>

#include <time/timer.h>

#include <assert.h>
#include <errors.h>

extern void sched_internal(bool state);
extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);
extern void waitq_do_wake(thread_t *thread);

/** Handle a timeout on a wait queue.
 * @param _thread	Pointer to thread that timed out.
 * @return		Whether to reschedule. */
static bool waitq_timer_handler(void *_thread) {
	thread_t *thread = _thread;

	spinlock_lock(&thread->lock, 0);

	/* Restore the interruption context and queue it to run. */
	thread->timed_out = true;
	thread->context = thread->sleep_context;
	waitq_do_wake(thread);

	spinlock_unlock(&thread->lock);
	return false;
}

/** Wake up a single thread. Thread and queue should be locked.
 * @param thread	Thread to wake up. */
void waitq_do_wake(thread_t *thread) {
	assert(thread->state == THREAD_SLEEPING);

	/* Stop the timer. */
	timer_stop(&thread->sleep_timer);

	/* Remove the thread from the queue and wake it up. */
	list_remove(&thread->waitq_link);
	thread->waitq = NULL;
	thread->interruptible = false;

	thread->state = THREAD_READY;
	sched_thread_insert(thread);
}

/** Sleep on a wait queue.
 *
 * Inserts the current thread into the specified wait queue and then sleeps
 * until it is woken by waitq_wake(). If the wait queue was created with
 * WAITQ_COUNT_MISSED, then the SYNC_NONBLOCK flag will cause the function to
 * return false if there is not a missed wakeup available. Otherwise, it will
 * have no effect.
 *
 * @param wait		Wait queue to wait on.
 * @param mtx		Optional mutex to unlock before sleeping, and relock
 *			after sleeping. This will not be used if SYNC_NONBLOCK
 *			is specified. The mutex will always be locked when the
 *			function returns - the flags specified will not be
 *			passed to mutex_lock() when relocking.
 * @param sl		Same as mtx but for a spinlock. You must not specify
 *			both a spinlock and a mutex.
 * @param timeout	Timeout in microseconds. A timeout of 0 has the same
 *			effect as SYNC_NONBLOCK (the reason both are provided
 *			is to let non-blocking mode be set for higher-level
 *			functions that don't allow a timeout parameter), and a
 *			timeout of -1 will sleep forever until the thread is
 *			woken.
 * @param flags		Synchronization flags (see sync/flags.h)
 *
 * @return		0 on success, negative error code on failure.
 */
int waitq_sleep(waitq_t *waitq, mutex_t *mtx, spinlock_t *sl, timeout_t timeout, int flags) {
	bool state = intr_disable();
	int ret = 0;

	assert(!(mtx && sl));

	spinlock_lock_ni(&waitq->lock, 0);

	/* Check if any missed wakeups are available. */
	if(waitq->flags & WAITQ_COUNT_MISSED) {
		if(waitq->missed > 0) {
			waitq->missed--;

			spinlock_unlock_ni(&waitq->lock);
			intr_restore(state);
			return 0;
		} else if(flags & SYNC_NONBLOCK || timeout == 0) {
			spinlock_unlock_ni(&waitq->lock);
			intr_restore(state);
			return -ERR_WOULD_BLOCK;
		}
	}

	/* We're going to sleep, now that the wait queue lock is held we can
	 * drop any locks we've been asked to drop. */
	if(mtx) {
		mutex_unlock(mtx);
	} else if(sl) {
		assert(!state);
		spinlock_unlock_ni(sl);
	}

	spinlock_lock_ni(&curr_thread->lock, 0);

	curr_thread->waitq = waitq;
	curr_thread->timed_out = false;

	/* Set up interruption/timeout context if required. */
	if(flags & SYNC_INTERRUPTIBLE || timeout > 0) {
		if(context_save(&curr_thread->sleep_context) != 0) {
			ret = (curr_thread->timed_out) ? -ERR_TIMED_OUT : -ERR_INTERRUPTED;
			sched_post_switch(state);
			goto out;
		}
	}

	/* Set whether we're interruptible, and set up a timeout if needed.
	 * Always initialise the timer structure as it gets checked in the
	 * wakeup functions. */
	curr_thread->interruptible = ((flags & SYNC_INTERRUPTIBLE) != 0);
	timer_init(&curr_thread->sleep_timer, waitq_timer_handler, curr_thread);
	if(timeout > 0) {
		timer_start(&curr_thread->sleep_timer, timeout);
	}

	/* Add the thread to the queue and unlock it. */
	list_append(&waitq->threads, &curr_thread->waitq_link);
	spinlock_unlock_ni(&waitq->lock);

	/* Send the thread to sleep. The scheduler will handle interrupt state
	 * and thread locking. */
	curr_thread->state = THREAD_SLEEPING;
	sched_internal(state);
out:
	if(mtx) {
		mutex_lock(mtx, 0);
	} else if(sl) {
		assert(!intr_state());
		spinlock_lock_ni(sl, 0);
	}
	return ret;
}

/** Wake up threads on a wait queue.
 *
 * Wakes up one or all threads currently waiting on a wait queue. If the queue
 * has the WAITQ_COUNT_MISSED flag set, the missed count will not be updated if
 * attempting to wake all threads.
 *
 * @param waitq		Wait queue to wake from.
 * @param all		Whether to wake all threads.
 *
 * @return		True if anything was woken, false if queue was empty.
 */
bool waitq_wake(waitq_t *waitq, bool all) {
	bool woken = false;
	thread_t *thread;

	spinlock_lock(&waitq->lock, 0);

	while(!list_empty(&waitq->threads)) {
		thread = list_entry(waitq->threads.next, thread_t, waitq_link);

		spinlock_lock(&thread->lock, 0);

		assert(thread->state == THREAD_SLEEPING);

		/* Remove the thread from the queue and wake it up. */
		waitq_do_wake(thread);
		spinlock_unlock(&thread->lock);

		woken = true;
		if(!all) {
			break;
		}
	}

	if(!woken && !all && waitq->flags & WAITQ_COUNT_MISSED) {
		waitq->missed++;
	}

	spinlock_unlock(&waitq->lock);
	return woken;
}

/** Check if a wait queue is empty.
 *
 * Checks if a wait queue is empty.
 *
 * @param waitq		Wait queue to check.
 *
 * @return		True if empty, false if not.
 */
bool waitq_empty(waitq_t *waitq) {
	bool ret;

	spinlock_lock(&waitq->lock, 0);
	ret = list_empty(&waitq->threads);
	spinlock_unlock(&waitq->lock);

	return ret;
}

/** Initialise a wait queue.
 *
 * Initialises the specified wait queue structure.
 *
 * @param waitq		Wait queue to initialise.
 * @param name		Name of wait queue.
 * @param flags		Flags for the queue.
 */
void waitq_init(waitq_t *waitq, const char *name, int flags) {
	spinlock_init(&waitq->lock, "waitq_lock");
	list_init(&waitq->threads);

	waitq->flags = flags;
	waitq->missed = 0;
	waitq->name = name;
}
