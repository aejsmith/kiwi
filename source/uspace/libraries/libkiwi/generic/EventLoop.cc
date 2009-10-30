/* Kiwi event loop class
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Kiwi event loop class.
 */

#include <kernel/handle.h>

#include <kiwi/private/EventLoop.h>

#include <exception>
#include <iostream>

using namespace kiwi;
using namespace std;

/* FIXME. */
EventLoop *global_event_loop = 0;

/** EventLoop constructor. */
EventLoop::EventLoop() : Object(new EventLoop::Private) {
	if(global_event_loop) {
		/* FIXME. */
		throw std::exception();
	}

	global_event_loop = this;
}

/** Add a handle to the event loop.
 * @param handle	Handle to use.
 * @param event		Event to wait for on the handle. */
void EventLoop::AddHandle(Handle *handle, int event) {
	Private *p = GetPrivate();

	p->m_handles.push_back(handle);
	p->m_ids.push_back(handle->GetHandle());
	p->m_events.push_back(event);

	handle->OnClose.Connect(this, &EventLoop::_HandleClosed);
}

/** Remove a handle from the event loop.
 * @param handle	Handle to remove.
 * @param event		Event that should be removed. */
void EventLoop::RemoveHandle(Handle *handle, int event) {
	vector<Handle *>::iterator it;
	Private *p = GetPrivate();
	size_t i;

	for(i = 0; i < p->m_handles.size(); i++) {
		if(p->m_handles[i] == handle && p->m_events[i] == event) {
			p->m_handles.erase(p->m_handles.begin() + i);
			p->m_ids.erase(p->m_ids.begin() + i);
			p->m_events.erase(p->m_events.begin() + i);
		}
	}
}

/** Run the event loop. */
void EventLoop::Run(void) {
	Private *p = GetPrivate();
	int ret;

	while(true) {
		if((ret = handle_wait_multiple(&p->m_ids[0], &p->m_events[0], p->m_handles.size(), -1)) < 0) {
			cerr << "Failed to wait for events (" << ret << ')' << endl;
			return;
		}

		p->m_handles[ret]->_EventReceived(p->m_events[ret]);
	}
}

/** Removes all events registered to a handle being closed. */
void EventLoop::_HandleClosed(Event &event) {
	Handle *handle = static_cast<Handle *>(event.GetObject());
	vector<Handle *>::iterator it;
	Private *p = GetPrivate();
	size_t i;

	for(i = 0; i < p->m_handles.size(); i++) {
		if(p->m_handles[i] == handle) {
			p->m_handles.erase(p->m_handles.begin() + i);
			p->m_ids.erase(p->m_ids.begin() + i);
			p->m_events.erase(p->m_events.begin() + i);
		}
	}
}
