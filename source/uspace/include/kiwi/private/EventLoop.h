/* Kiwi event loop class private data
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
 * @brief		Kiwi event loop class private data.
 */

#ifndef __KIWI_PRIVATE_EVENTLOOP_H
#define __KIWI_PRIVATE_EVENTLOOP_H

#include <kiwi/private/Object.h>
#include <kiwi/EventLoop.h>

#include <vector>

namespace kiwi {

/** EventLoop class private data. */
class EventLoop::Private : public Object::Private {
	KIWI_OBJECT_PUBLIC(EventLoop);
private:
	/** @note Data stored as multiple vectors because it is the format
	 *        handle_wait_multiple() expects. */
	std::vector<Handle *> m_handles;	/**< Array of handle objects (used for callbacks). */
	std::vector<handle_t> m_ids;		/**< Array of handle IDs. */
	std::vector<int> m_events;		/**< Array of events to wait for. */
};

}

#endif /* __KIWI_PRIVATE_EVENTLOOP_H */
