/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Event loop class.
 */

#include <kernel/object.h>
#include <kernel/status.h>

#include <kiwi/Support/Mutex.h>
#include <kiwi/EventLoop.h>

#include <list>
#include <vector>

#include "Internal.h"

using namespace kiwi;
using namespace std;

/** Internal data for EventLoop. */
struct kiwi::EventLoopPrivate {
	EventLoopPrivate() : quit(false), status(0) {}

	std::list<Object *> to_delete;		/**< Objects to delete when control returns to the loop. */
	std::vector<object_event_t> events;	/**< Array of events to wait for. */
	std::vector<Handle *> handles;		/**< Array of handle objects (used for callbacks). */

	bool quit;				/**< Whether to quit the event loop. */
	int status;				/**< Exit status. */
};

extern __thread EventLoop *g_esdfsfvent_loop;
__thread EventLoop *g_esdfsfvent_loop = 0;
/** Pointer to the current thread's event loop. */
__thread EventLoop *g_event_loop = 0;

/** Event loop constructor. */
EventLoop::EventLoop() :
	m_priv(new EventLoopPrivate)
{
	if(g_event_loop) {
		libkiwi_fatal("EventLoop::EventLoop: Can only have 1 event loop per thread.");
	} else {
		g_event_loop = this;
	}
}

/** Event loop constructor for use by Thread.
 * @note		This is an internal constructor for use by Thread. It
 *			does not check or set the global event loop pointer.
 *			This is because Thread creates the event loop along
 *			with the Thread object, and sets the event loop pointer
 *			itself in the thread entry function. */
EventLoop::EventLoop(bool priv) : m_priv(new EventLoopPrivate) {}

/** EventLoop destructor. */
EventLoop::~EventLoop() {
	delete m_priv;
	if(g_event_loop == this) {
		g_event_loop = 0;
	}
}

/** Add an event to the event loop.
 * @param handle	Handle the event will come from.
 * @param event		Event to wait for. */
void EventLoop::AddEvent(Handle *handle, int event) {
	object_event_t _event = { handle->GetHandle(), event, false };
	m_priv->events.push_back(_event);
	m_priv->handles.push_back(handle);
}

/** Remove an event from the event loop.
 * @param handle	Handle to event is from.
 * @param event		Event that should be removed. */
void EventLoop::RemoveEvent(Handle *handle, int event) {
	for(size_t i = 0; i < m_priv->handles.size(); ) {
		if(m_priv->handles[i] == handle && m_priv->events[i].event == event) {
			m_priv->handles.erase(m_priv->handles.begin() + i);
			m_priv->events.erase(m_priv->events.begin() + i);
		} else {
			i++;
		}
	}
}

/** Removes all events for a handle.
 * @param handle	Handle to remove. */
void EventLoop::RemoveHandle(Handle *handle) {
	for(size_t i = 0; i < m_priv->handles.size(); ) {
		if(m_priv->handles[i] == handle) {
			m_priv->handles.erase(m_priv->handles.begin() + i);
			m_priv->events.erase(m_priv->events.begin() + i);
		} else {
			i++;
		}
	}
}

/** Run the event loop.
 * @return		Status code the event loop was asked to exit with. */
int EventLoop::Run(void) {
	m_priv->status = 0;
	m_priv->quit = false;

	while(true) {
		status_t ret;

		/* Delete objects scheduled for deletion. */
		list<Object *>::iterator it;
		while((it = m_priv->to_delete.begin()) != m_priv->to_delete.end()) {
			delete *it;
			m_priv->to_delete.erase(it);
		}

		/* If we have nothing to do, or we have been asked to, exit. */
		if(!m_priv->handles.size() || m_priv->quit) {
			return m_priv->status;
		}

		/* Wait for any of the events. */
		ret = object_wait(&m_priv->events[0], m_priv->handles.size(), -1);
		if(unlikely(ret != STATUS_SUCCESS)) {
			libkiwi_fatal("EventLoop::Run: Failed to wait for events: %d", ret);
		}

		/* Signal each handle an event occurred on. */
		for(size_t i = 0; i < m_priv->events.size(); i++) {
			if(m_priv->events[i].signalled) {
				m_priv->handles[i]->HandleEvent(m_priv->events[i].event);
			}
		}
	}
}

/** Ask the event loop to quit.
 * @param status	Status code to make the event loop return.
 * @todo		If the event loop is currently in object_wait(), we
 *			should wake it up somehow. */
void EventLoop::Quit(int status) {
	m_priv->status = status;
	m_priv->quit = true;
}

/** Get the current thread's event loop.
 * @return		Pointer to the current thread's event loop, or NULL if
 *			the thread does not have an event loop. */
EventLoop *EventLoop::Instance() {
	return g_event_loop;
}

/** Register an object to be deleted when control returns to the event loop.
 * @param obj		Object to delete. */
void EventLoop::DeleteObject(Object *obj) {
	m_priv->to_delete.push_back(obj);
}
