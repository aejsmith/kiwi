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
 * @brief		Kiwi event loop class.
 */

#ifndef __KIWI_EVENTLOOP_H
#define __KIWI_EVENTLOOP_H

#include <kernel/object.h>

#include <kiwi/Handle.h>
#include <kiwi/Object.h>

#include <list>
#include <vector>

namespace kiwi {

/** Class implementing a loop for handling object events.
 * @todo		When threading support is implemented, each thread
 *			should have its own event loop, and Instance() should
 *			return the calling thread's event loop. */
class EventLoop : public Object, internal::Noncopyable {
public:
	EventLoop();

	void AddEvent(Handle *handle, int event);
	void RemoveEvent(Handle *handle, int event);
	void RemoveHandle(Handle *handle);
	void DeleteObject(Object *obj);
	void Run();

	static EventLoop *Instance();
private:
	std::list<Object *> m_to_delete;	/**< Objects to delete when control returns to the loop. */

	std::vector<Handle *> m_handles;	/**< Array of handle objects (used for callbacks). */
	std::vector<object_event_t> m_events;	/**< Array of events to wait for. */
};

}

#endif /* __KIWI_EVENTLOOP_H */
