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
 * @brief		Handle class.
 */

#include <kernel/object.h>

#include <kiwi/EventLoop.h>
#include <kiwi/Handle.h>

#include <cstdlib>
#include <iostream>

using namespace kiwi;
using namespace std;

extern EventLoop *global_event_loop;

/** Handle constructor. */
Handle::Handle(handle_t handle) : m_handle(handle) {}

/** Destructor to close the handle. */
Handle::~Handle() {
	close();
}

/** Close the handle. */
void Handle::close() {
	if(m_handle >= 0) {
		onClose(this);

		if(handle_close(m_handle) != 0) {
			/* The only reason for failure in handle_close is the
			 * handle not existing. Therefore, if we fail, it means
			 * the programmer has done something funny so we just
			 * throw up a warning and act as though it has already
			 * been closed. */
			cerr << "Warning: Handle " << m_handle << " has already been closed" << endl;
		}

		m_handle = -1;
	}
}

/** Wait for an event on the object referred to by the handle.
 * @note		Derived classes should implement their own functions to
 *			wait for events on top of this function, which should
 *			be used rather than using this function directly.
 * @param event		Event to wait for (specific to handle type).
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the event has not already
 *			happened, and a value of -1 (the default) will block
 *			indefinitely until the event happens.
 * @return		True on success, false on failure. */
bool Handle::wait(int event, useconds_t timeout) const {
	return (object_wait(m_handle, event, timeout) == 0);
}

/** Get the ID of the handle.
 * @return		ID of the handle. */
handle_t Handle::getHandle(void) const {
	return m_handle;
}

/** Register an event with the current thread's event loop.
 * @param event		Event ID to register. */
void Handle::registerEvent(int event) {
	if(global_event_loop) {
		global_event_loop->addHandle(this, event);
	}
}

/** Unregister an event with the current thread's event loop.
 * @param event		Event ID to unregister. */
void Handle::unregisterEvent(int event) {
	if(global_event_loop) {
		global_event_loop->removeHandle(this, event);
	}
}
