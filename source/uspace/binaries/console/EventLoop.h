/* Kiwi console event loop
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
 * @brief		Console event loop.
 */

#ifndef __EVENTLOOP_H
#define __EVENTLOOP_H

#include <kernel/handle.h>

/** Class implementing a loop to act on handle events. */
class EventLoop {
public:
	/** Type of a callback function.
	 * @param data		Data argument. */
	typedef void (*Callback)(void *data);

	void Run(void);
	void AddHandle(handle_t handle, int event, Callback callback, void *data);

	/** Retreive the singleton instance.
	 * @return		Pointer to instance. */
	static EventLoop *Instance(void) { return &m_instance; }
private:
	EventLoop();

	static EventLoop m_instance;	/**< Singleton instance. */

	/** @note Data stored as multiple arrays rather than as a single array
	 * of structures because it is the format handle_wait_multiple()
	 * expects. */
	handle_t *m_handles;		/**< Array of handles. */
	int *m_events;			/**< Array of events to wait for. */
	Callback *m_callbacks;		/**< Array of callback functions. */
	void **m_datas;			/**< Array of data arguments. */
	size_t m_count;			/**< Count of handles. */
};

#endif /* __EVENTLOOP_H */
