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

#ifndef __KIWI_EVENTLOOP_H
#define __KIWI_EVENTLOOP_H

#include <kiwi/Handle.h>
#include <kiwi/Object.h>

#include <list>
#include <vector>

namespace kiwi {

/** Class implementing a loop for handling handle events.
 * @todo		When threading support is implemented, each thread
 *			should have its own event loop, and handles should be
 *			added to the event loop of the thread they are created
 *			in. */
class EventLoop : public Object {
	KIWI_OBJECT_NONCOPYABLE(EventLoop);
public:
	EventLoop();

	void AddHandle(Handle *handle, int event);
	void RemoveHandle(Handle *handle, int event);
	void DeleteObject(Object *obj);
	void Run();
private:
	void _HandleClosed(Handle *handle);

	std::list<Object *> m_to_delete;	/**< Objects to delete when control returns to the loop. */

	/** @note Data stored as multiple vectors because it is the format
	 *        handle_wait_multiple() expects. */
	std::vector<Handle *> m_handles;	/**< Array of handle objects (used for callbacks). */
	std::vector<handle_t> m_ids;		/**< Array of handle IDs. */
	std::vector<int> m_events;		/**< Array of events to wait for. */
};

}

#endif /* __KIWI_EVENTLOOP_H */
