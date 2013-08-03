/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Thread class.
 */

#include <kernel/object.h>
#include <kernel/thread.h>

#include <kiwi/Thread.h>

#include <cassert>
#include <string>

#include "Internal.h"

using namespace kiwi;

/** Internal data for Thread. */
struct kiwi::ThreadPrivate {
	ThreadPrivate() : name("user_thread"), event_loop(0) {}

	std::string name;		/**< Name to give the thread. */
	EventLoop *event_loop;		/**< Event loop for the thread. */
};

/** Set up the thread object.
 * @note		The thread is not created here. Once the object has
 *			been initialised, you can either open an existing
 *			thread using Open(), or start a new thread using
 *			Run().
 * @param handle	If not negative, a existing thread handle to make the
 *			object use. Must refer to a thread object. */
Thread::Thread(handle_t handle) :
	m_priv(new ThreadPrivate)
{
	if(handle >= 0) {
		if(unlikely(kern_object_type(handle) != OBJECT_TYPE_THREAD)) {
			libkiwi_fatal("Thread::Thread: Handle must refer to a thread object.");
		}

		SetHandle(handle);
	}

	m_priv->event_loop = new EventLoop(true);
}

/** Destroy the thread object.
 *
 * Destroys the thread object. It should not be running. If any handles are
 * still attached to the thread's event loop, they will be moved to the calling
 * thread's event loop.
 */
Thread::~Thread() {
	assert(!IsRunning());

	if(m_priv->event_loop) {
		EventLoop *current = EventLoop::Instance();
		if(current) {
			/* Move handles from the thread's event loop to the
			 * current thread's. */
			current->Merge(m_priv->event_loop);
		} else {
			/* Throw up a warning: when we destroy the loop the
			 * handles it contains will have an invalid event loop
			 * pointer. FIXME: Only do this if there are handles in
			 * the loop. */
			libkiwi_warn("Thread::~Thread: No event loop to move handles to.");
		}

		delete m_priv->event_loop;
	}

	delete m_priv;
}

/** Open an existing thread.
 * @param id		ID of thread to open.
 * @return		True if succeeded in opening thread, false if not. More
 *			information about an error can be retrieved by calling
 *			GetError(). */
bool Thread::Open(thread_id_t id) {
	handle_t handle;
	status_t ret;

	ret = kern_thread_open(id, THREAD_RIGHT_QUERY, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Set the name to use for a new thread.
 * @param name		Name to use for the thread. */
void Thread::SetName(const char *name) {
	m_priv->name = name;
}

/** Start the thread.
 * @return		True if succeeded in creating thread, false if not.
 *			More information about an error can be retrieved by
 *			calling GetError(). */
bool Thread::Run() {
	handle_t handle;
	status_t ret;

	ret = kern_thread_create(m_priv->name.c_str(), NULL, 0, &Thread::_Entry, this,
	                         NULL, THREAD_RIGHT_QUERY, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Wait for the thread to exit.
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the thread has not already exited,
 *			and a value of -1 will block indefinitely until the
 *			thread exits.
 * @return		True if thread exited within the timeout, false if not. */
bool Thread::Wait(useconds_t timeout) const {
	return (_Wait(THREAD_EVENT_DEATH, timeout) == STATUS_SUCCESS);
}

/** Ask the thread to quit.
 * @param status	Status to make the thread's event loop return with. */
void Thread::Quit(int status) {
	if(IsRunning()) {
		m_priv->event_loop->Quit(status);
	}
}

/** Check whether the thread is running.
 * @return		Whether the thread is running. */
bool Thread::IsRunning() const {
	int status;
	return (m_handle >= 0 && kern_thread_status(m_handle, &status) == STATUS_STILL_RUNNING);
}

/** Get the exit status of the thread.
 * @return		Exit status of the thread, or -1 if still running. */
int Thread::GetStatus() const {
	int status;

	if(kern_thread_status(m_handle, &status) != STATUS_SUCCESS) {
		return -1;
	}
	return status;
}

/** Get the ID of the thread.
 * @return		ID of the thread. */
thread_id_t Thread::GetID() const {
	return kern_thread_id(m_handle);
}

/** Get the ID of the current thread.
 * @return		ID of the current thread. */
thread_id_t Thread::GetCurrentID() {
	return kern_thread_id(-1);
}

/** Sleep for a certain time period.
 * @param usecs		Microseconds to sleep for. */
void Thread::Sleep(useconds_t usecs) {
	kern_thread_usleep(usecs, NULL);
}

/** Get the thread's event loop.
 * @return		Reference to thread's event loop. */
EventLoop &Thread::GetEventLoop() {
	return *m_priv->event_loop;
}

/** Main function for the thread.
 *
 * The main function for the thread, which is called when the thread starts
 * running. This can be overridden by derived classes to do their own work.
 * The default implementation just runs the thread's event loop.
 *
 * @return		Exit status code for the thread.
 */
int Thread::Main() {
	return m_priv->event_loop->Run();
}

/** Register events with the event loop. */
void Thread::RegisterEvents() {
	RegisterEvent(THREAD_EVENT_DEATH);
}

/** Handle an event from the thread.
 * @param event		Event ID. */
void Thread::HandleEvent(int event) {
	if(event == THREAD_EVENT_DEATH) {
		int status = 0;
		kern_thread_status(m_handle, &status);
		OnExit(status);

		/* Unregister the death event so that it doesn't continually
		 * get signalled. */
		UnregisterEvent(THREAD_EVENT_DEATH);
	}
}

/** Entry point for a new thread.
 * @param arg		Pointer to Thread object. */
void Thread::_Entry(void *arg) {
	Thread *thread = reinterpret_cast<Thread *>(arg);

	/* Set the event loop pointer. */
	g_event_loop = thread->m_priv->event_loop;

	/* Call the main function. */
	kern_thread_exit(thread->Main());
}