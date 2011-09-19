/*
 * Copyright (C) 2008-2011 Alex Smith
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

extern void thread_wake(thread_t *thread);

/**
 * Prepare to sleep on a wait queue.
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
	assert(spinlock_held(&queue->lock));
	assert(!intr_state());

	if(!timeout) {
		spinlock_unlock_ni(&queue->lock);
		intr_restore(state);
		return STATUS_WOULD_BLOCK;
	}

	spinlock_lock_ni(&curr_thread->lock);

	/* Save details of the sleep to the thread structure. */
	curr_thread->waitq = queue;
	curr_thread->interruptible = ((flags & SYNC_INTERRUPTIBLE) != 0);
	curr_thread->sleep_status = STATUS_SUCCESS;

	/* Set up a timeout if needed. */
	if(timeout > 0) {
		timer_start(&curr_thread->sleep_timer, timeout, TIMER_ONESHOT);
	}

	/* Add the thread to the queue and unlock it. */
	list_append(&queue->threads, &curr_thread->waitq_link);
	spinlock_unlock_ni(&queue->lock);

	/* Send the thread to sleep. The scheduler will handle interrupt state
	 * and thread locking. */
	curr_thread->state = THREAD_SLEEPING;
	sched_reschedule(state);
	return curr_thread->sleep_status;
}

/**
 * Sleep on a wait queue.
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
	bool state = waitq_sleep_prepare(queue);
	return waitq_sleep_unsafe(queue, timeout, flags, state);
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
