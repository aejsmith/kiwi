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

#include <kiwi/EventLoop.h>

#include <list>
#include <vector>

#include "Internal.h"

using namespace kiwi;
using namespace std;

KIWI_BEGIN_NAMESPACE

/** Internal data for EventLoop. */
struct EventLoopPrivate {
	std::list<Object *> to_delete;		/**< Objects to delete when control returns to the loop. */
	std::vector<object_event_t> events;	/**< Array of events to wait for. */
	std::vector<Handle *> handles;		/**< Array of handle objects (used for callbacks). */
};

KIWI_END_NAMESPACE

/** Pointer to the current thread's event loop. */
static __thread EventLoop *event_loop_instance = 0;

/** EventLoop constructor. */
EventLoop::EventLoop() {
	if(event_loop_instance) {
		libkiwi_fatal("EventLoop::EventLoop: Can only have 1 event loop per thread.");
	}

	m_priv = new EventLoopPrivate;
	event_loop_instance = this;
}

/** EventLoop destructor. */
EventLoop::~EventLoop() {
	delete m_priv;
	event_loop_instance = 0;
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

/** Run the event loop. */
void EventLoop::Run(void) {
	while(true) {
		/* Delete objects scheduled for deletion. */
		list<Object *>::iterator it;
		while((it = m_priv->to_delete.begin()) != m_priv->to_delete.end()) {
			delete *it;
			m_priv->to_delete.erase(it);
		}

		/* Wait for any of the events. */
		status_t ret = object_wait(&m_priv->events[0], m_priv->handles.size(), -1);
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

/** Get the current thread's event loop.
 * @return		Pointer to the current thread's event loop, or NULL if
 *			the thread does not have an event loop. */
EventLoop *EventLoop::Instance() {
	return event_loop_instance;
}

/** Register an object to be deleted when control returns to the event loop.
 * @param obj		Object to delete. */
void EventLoop::DeleteObject(Object *obj) {
	m_priv->to_delete.push_back(obj);
}
