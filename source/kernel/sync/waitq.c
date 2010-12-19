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
 * @brief		Wait queue functions.
 */

#include <cpu/intr.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/waitq.h>

#include <assert.h>
#include <status.h>
#include <time.h>

extern void sched_internal(bool state);
extern void sched_post_switch(bool state);
extern void thread_wake(thread_t *thread);

/** Handle a timeout on a wait queue.
 * @param _thread	Pointer to thread that timed out.
 * @return		Whether to preempt. */
static bool waitq_timer_handler(void *_thread) {
	thread_t *thread = _thread;
	waitq_t *queue;

	spinlock_lock(&thread->lock);

	/* The thread could have been woken up already by another CPU. */
	if(thread->state == THREAD_SLEEPING) {
		/* Restore the interruption context and queue it to run. */
		queue = thread->waitq;
		spinlock_lock(&queue->lock);
		thread->timed_out = true;
		thread->context = thread->sleep_context;
		thread_wake(thread);
		spinlock_unlock(&queue->lock);
	}

	spinlock_unlock(&thread->lock);
	return false;
}

/** Prepare to sleep on a wait queue.
 *
 * Prepares for the current thread to sleep on a wait queue. The wait queue
 * lock will be taken and interrupts will be disabled. To begin waiting after
 * calling this function, use waitq_sleep_unsafe(). This function should only
 * be used if it is necessary to perform special tasks between taking the wait
 * queue lock and going to sleep (for an example, see condvar_wait()); if no
 * special behaviour is required, use waitq_sleep().
 *
 * @param queue		Queue to lock.
 *
 * @return		Previous interrupt state.
 */
bool waitq_sleep_prepare(waitq_t *queue) {
	bool state = intr_disable();
	spinlock_lock_ni(&queue->lock);
	return state;
}

/** Cancel a prepared sleep.
 * @param queue		Queue to cancel from.
 * @param state		Interrupt state to restore. */
void waitq_sleep_cancel(waitq_t *queue, bool state) {
	spinlock_unlock_ni(&queue->lock);
	intr_restore(state);
}

/** Sleep on a wait queue.
 * @see			waitq_sleep_prepare().
 * @param queue		Queue to sleep on.
 * @param timeout	Timeout in microseconds. If 0 is specified, then the
 *			function will return an error immediately. If -1, it
 *			will block indefinitely until the thread is woken.
 * @param flags		Flags to modify behaviour (see sync/flags.h).
 * @param state		Interrupt state returned from waitq_sleep_prepare().
 * @return		Status code describing result of the operation. */
status_t waitq_sleep_unsafe(waitq_t *queue, useconds_t timeout, int flags, bool state) {
	status_t ret;

	assert(spinlock_held(&queue->lock));
	assert(!intr_state());

	if(!timeout) {
		spinlock_unlock_ni(&queue->lock);
		intr_restore(state);
		return STATUS_WOULD_BLOCK;
	}

	spinlock_lock_ni(&curr_thread->lock);

	curr_thread->waitq = queue;
	curr_thread->timed_out = false;

	/* Set up interruption/timeout context if necessary. This context will
	 * be restored if sleep is interrupted. */
	if(flags & SYNC_INTERRUPTIBLE || timeout > 0) {
		if(context_save(&curr_thread->sleep_context)) {
			ret = (curr_thread->timed_out) ? STATUS_TIMED_OUT : STATUS_INTERRUPTED;
			sched_post_switch(state);
			return ret;
		}
	}

	/* Set whether we're interruptible, and set up a timeout if needed. */
	curr_thread->interruptible = ((flags & SYNC_INTERRUPTIBLE) != 0);
	timer_init(&curr_thread->sleep_timer, waitq_timer_handler, curr_thread, 0);
	if(timeout > 0) {
		timer_start(&curr_thread->sleep_timer, timeout, TIMER_ONESHOT);
	}

	/* Add the thread to the queue and unlock it. */
	list_append(&queue->threads, &curr_thread->waitq_link);
	spinlock_unlock_ni(&queue->lock);

	/* Send the thread to sleep. The scheduler will handle interrupt state
	 * and thread locking. */
	curr_thread->state = THREAD_SLEEPING;
	sched_internal(state);
	return STATUS_SUCCESS;
}

/** Sleep on a wait queue.
 *
 * Inserts the current thread into a wait queue and then sleeps until woken
 * by waitq_wake()/waitq_wake_all(), until the timeout specified expires, or,
 * if the SYNC_INTERRUPTIBLE flag is set, until interrupted.
 *
 * @param queue		Queue to sleep on.
 * @param timeout	Timeout in microseconds. If 0 is specified, then the
 *			function will return an error immediately. If -1, it
 *			will block indefinitely until the thread is woken.
 * @param flags		Flags to modify behaviour (see sync/flags.h).
 *
 * @return		Status code describing result of the operation. Failure
 *			is only possible if the timeout is not -1, or if the
 *			SYNC_INTERRUPTIBLE flag is set.
 */
status_t waitq_sleep(waitq_t *queue, useconds_t timeout, int flags) {
	return waitq_sleep_unsafe(queue, timeout, flags, waitq_sleep_prepare(queue));
}

/** Wake up the next thread on a wait queue.
 * @param queue		Queue to wake from. Will NOT be locked or unlocked.
 * @return		Whether a thread was woken. */
bool waitq_wake_unsafe(waitq_t *queue) {
	thread_t *thread;

	if(!list_empty(&queue->threads)) {
		thread = list_entry(queue->threads.next, thread_t, waitq_link);
		spinlock_lock(&thread->lock);
		thread_wake(thread);
		spinlock_unlock(&thread->lock);
		return true;
	} else {
		return false;
	}
}

/** Wake up the next thread on a wait queue.
 * @param queue		Queue to wake from.
 * @return		Whether a thread was woken. */
bool waitq_wake(waitq_t *queue) {
	bool ret;

	spinlock_lock(&queue->lock);
	ret = waitq_wake_unsafe(queue);
	spinlock_unlock(&queue->lock);
	return ret;
}

/** Wake up all threads on a wait queue.
 * @param queue		Queue to wake from.
 * @return		Whether any threads were woken. */
bool waitq_wake_all(waitq_t *queue) {
	bool woken = false;

	spinlock_lock(&queue->lock);
	while(waitq_wake_unsafe(queue)) {
		woken = true;
	}
	spinlock_unlock(&queue->lock);
	return woken;
}

/** Check if a wait queue is empty.
 * @param queue		Wait queue to check.
 * @return		Whether the wait queue is empty. */
bool waitq_empty(waitq_t *queue) {
	bool ret;

	spinlock_lock(&queue->lock);
	ret = list_empty(&queue->threads);
	spinlock_unlock(&queue->lock);

	return ret;
}

/** Initialise a wait queue structure.
 * @param queue		Wait queue to initialise.
 * @param name		Name of wait queue. */
void waitq_init(waitq_t *queue, const char *name) {
	spinlock_init(&queue->lock, "waitq_lock");
	list_init(&queue->threads);
	queue->name = name;
}
