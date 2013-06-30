/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Event loop class.
 */

#include <kernel/object.h>
#include <kernel/status.h>

#include <kiwi/EventLoop.h>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <list>
#include <map>
#include <vector>

#include "Internal.h"

using namespace kiwi;
using namespace std;

/** Internal data for EventLoop. */
struct kiwi::EventLoopPrivate {
	EventLoopPrivate() : quit(false), status(0) {}

	/** Delete all objects in the to-delete list. */
	void DeleteObjects() {
		list<Object *>::iterator obj;
		while((obj = to_delete.begin()) != to_delete.end()) {
			delete *obj;
			to_delete.erase(obj);
		}
	}

	map<handle_t, Handle *> handles;	/**< Map of handles attached to the event loop. */
	vector<object_event_t> events;		/**< Array of events to wait for. */

	list<Object *> to_delete;		/**< Objects to delete when control returns to the loop. */

	bool quit;				/**< Whether to quit the event loop. */
	int status;				/**< Exit status. */
};

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
	/* Destroy any objects that remain in the to-delete list. */
	m_priv->DeleteObjects();

	if(g_event_loop == this) {
		g_event_loop = 0;
	}

	delete m_priv;
}

/** Attach a handle to the event loop.
 * @param handle	Handle to add. Must not already be in an event loop. */
void EventLoop::AttachHandle(Handle *handle) {
	assert(handle->m_event_loop == this);

	/* Add the handle to the handle map. */
	auto ret = m_priv->handles.insert(make_pair(handle->GetHandle(), handle));
	if(unlikely(!ret.second)) {
		libkiwi_fatal("EventLoop::AddHandle: Handle with same ID already in event loop.");
	}
}

/** Detach a handle from the event loop.
 * @param handle	Handle to remove. */
void EventLoop::DetachHandle(Handle *handle) {
	assert(handle->m_event_loop == this);

	/* Remove all events for the handle. */
	RemoveEvents(handle);

	/* Remove from the handle map. */
	if(unlikely(!m_priv->handles.erase(handle->GetHandle()))) {
		libkiwi_fatal("EventLoop::RemoveHandle: Could not find handle being removed.");
	}
}

/** Add an event to the event loop.
 * @param handle	Handle the event will come from.
 * @param event		Event to wait for. */
void EventLoop::AddEvent(Handle *handle, int event) {
	assert(handle->m_event_loop == this);

	object_event_t _event = { handle->GetHandle(), event, false };
	m_priv->events.push_back(_event);
}

/** Remove an event from the event loop.
 * @param handle	Handle to event is from.
 * @param event		Event that should be removed. */
void EventLoop::RemoveEvent(Handle *handle, int event) {
	assert(handle->m_event_loop == this);

	for(auto it = m_priv->events.begin(); it != m_priv->events.end(); ) {
		if(it->handle == handle->GetHandle() && it->event == event) {
			it = m_priv->events.erase(it);
		} else {
			++it;
		}
	}
}

/** Removes all events for a handle.
 * @param handle	Handle to remove for. */
void EventLoop::RemoveEvents(Handle *handle) {
	assert(handle->m_event_loop == this);

	for(auto it = m_priv->events.begin(); it != m_priv->events.end(); ) {
		if(it->handle == handle->GetHandle()) {
			it = m_priv->events.erase(it);
		} else {
			++it;
		}
	}
}

/** Perform pre-event handling tasks. */
void EventLoop::PreHandle() {}

/** Perform post-event handling tasks. */
void EventLoop::PostHandle() {}

/** Run the event loop.
 * @return		Status code the event loop was asked to exit with. */
int EventLoop::Run(void) {
	m_priv->status = 0;
	m_priv->quit = false;

	while(true) {
		status_t ret;

		/* Delete objects scheduled for deletion. */
		m_priv->DeleteObjects();

		/* If we have nothing to do, or we have been asked to, exit. */
		if(!m_priv->events.size() || m_priv->quit) {
			return m_priv->status;
		}

		/* Wait for any of the events to occur. */
		ret = kern_object_wait(&m_priv->events[0], m_priv->events.size(), -1);
		if(unlikely(ret != STATUS_SUCCESS)) {
			libkiwi_fatal("EventLoop::Run: Failed to wait for events: %d", ret);
		}

		PreHandle();

		/* Signal each handle an event occurred on. */
		for(size_t i = 0; i < m_priv->events.size(); i++) {
			if(m_priv->events[i].signalled) {
				auto handle = m_priv->handles.find(m_priv->events[i].handle);
				handle->second->HandleEvent(m_priv->events[i].event);
			}
		}

		PostHandle();
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

/** Move all handles and events from an existing event loop to this one.
 * @param old		Old event loop. */
void EventLoop::Merge(EventLoop *old) {
	assert(!g_event_loop || g_event_loop == this);

	/* Merge the handle map in. */
	for(auto it = old->m_priv->handles.begin(); it != old->m_priv->handles.end(); ++it) {
		assert(it->second->m_event_loop == old);
		it->second->m_event_loop = this;
		m_priv->handles.insert(*it);
	}

	/* Add the contents of the handle array to ours. */
	copy(old->m_priv->events.begin(), old->m_priv->events.end(), back_inserter(m_priv->events));
}

/** Register an object to be deleted when control returns to the event loop.
 * @param obj		Object to delete. */
void EventLoop::DeleteObject(Object *obj) {
	m_priv->to_delete.push_back(obj);
}
