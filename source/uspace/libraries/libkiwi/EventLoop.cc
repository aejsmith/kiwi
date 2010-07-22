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

#include <kiwi/private/log.h>
#include <kiwi/EventLoop.h>
#include <kiwi/Exception.h>

#include <cerrno>
#include <cstring>

using namespace kiwi;
using namespace std;

/** Pointer to the current thread's event loop.
 * @todo		Move this to TLS. */
static EventLoop *event_loop_instance = 0;

/** EventLoop constructor. */
EventLoop::EventLoop() {
	if(instance()) {
		throw Exception("Can only have 1 EventLoop per thread");
	}

	event_loop_instance = this;
}

/** Get the current thread's event loop.
 * @return		Pointer to the current thread's event loop, or NULL if
 *			the thread does not have an event loop. */
EventLoop *EventLoop::instance() {
	return event_loop_instance;
}

/** Add an event to the event loop.
 * @param handle	Handle the event will come from.
 * @param event		Event to wait for. */
void EventLoop::addEvent(Handle *handle, int event) {
	m_handles.push_back(handle);
	m_ids.push_back(handle->getHandle());
	m_events.push_back(event);
}

/** Remove an event from the event loop.
 * @param handle	Handle to event is from.
 * @param event		Event that should be removed. */
void EventLoop::removeEvent(Handle *handle, int event) {
	for(size_t i = 0; i < m_handles.size(); ) {
		if(m_handles[i] == handle && m_events[i] == event) {
			m_handles.erase(m_handles.begin() + i);
			m_ids.erase(m_ids.begin() + i);
			m_events.erase(m_events.begin() + i);
		} else {
			i++;
		}
	}
}

/** Removes all events for a handle.
 * @param handle	Handle to remove. */
void EventLoop::removeHandle(Handle *handle) {
	for(size_t i = 0; i < m_handles.size(); ) {
		if(m_handles[i] == handle) {
			m_handles.erase(m_handles.begin() + i);
			m_ids.erase(m_ids.begin() + i);
			m_events.erase(m_events.begin() + i);
		} else {
			i++;
		}
	}
}

/** Register an object to be deleted when control returns to the event loop.
 * @param obj		Object to delete. */
void EventLoop::deleteObject(Object *obj) {
	m_to_delete.push_back(obj);
}

/** Run the event loop. */
void EventLoop::run(void) {
	while(true) {
		/* Delete objects scheduled for deletion. */
		list<Object *>::iterator it;
		while((it = m_to_delete.begin()) != m_to_delete.end()) {
			delete *it;
			m_to_delete.erase(it);
		}

		/* Wait for any of the events. */
		int ret = object_wait_multiple(&m_ids[0], &m_events[0], m_handles.size(), -1);
		if(ret < 0) {
			lkWarning("EventLoop::run: Failed to wait for events: %s\n", strerror(errno));
			return;
		}

		/* Signal the handle the event occurred on. */
		m_handles[ret]->eventReceived(m_events[ret]);
	}
}
