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
#include <kiwi/private/log.h>

using namespace kiwi;

extern EventLoop *global_event_loop;

/** Handle constructor.
 * @note		ARGH RAGE!!! When a base class constructor is run the
 *			object acts as though it's an instance of the base
 *			class, not the derived class. That means that the
 *			object's vtable will point to Handle's, not the derived
 *			class's. Because of this, calling setHandle here will
 *			not register events because the registerEvents that
 *			will be used is Handle's, not the derived class's. So,
 *			we must do the setHandle in all derived class's
 *			constructors.
 *			http://www.artima.com/cppsource/nevercall.html
 *			http://www.artima.com/cppsource/pure_virtual.html */
Handle::Handle() : m_handle(-1) {}

/** Destructor to close the handle. */
Handle::~Handle() {
	close();
}

/** Close the handle. */
void Handle::close() {
	if(m_handle >= 0) {
		/* Remove all events for this handle from the event loop and
		 * run callbacks for the handle being closed. */
		if(global_event_loop) {
			global_event_loop->removeHandle(this);
		}
		onClose(this);

		/* The only error handle_close() can encounter is the handle
		 * not existing. Therefore, if we fail, it means the programmer
		 * has done something funny so we just throw up a warning and
		 * act as though it has already been closed. */
		if(handle_close(m_handle) != 0) {
			lkWarning("Handle::close: Handle %d has already been closed\n", m_handle);
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

/** Get the kernel handle the handle is using.
 * @return		Raw handle. */
handle_t Handle::getHandle(void) const {
	return m_handle;
}

/** Set the kernel handle to use.
 * @param handle	New handle. The current handle (if any) will be closed. */
void Handle::setHandle(handle_t handle) {
	close();
	m_handle = handle;
	if(m_handle >= 0) {
		registerEvents();
	}
}

/** Register all events that the event loop should poll for. */
void Handle::registerEvents() {}

/** Register an event with the current thread's event loop.
 * @param event		Event ID to register. */
void Handle::registerEvent(int event) {
	if(global_event_loop) {
		global_event_loop->addEvent(this, event);
	}
}

/** Unregister an event with the current thread's event loop.
 * @param event		Event ID to unregister. */
void Handle::unregisterEvent(int event) {
	if(global_event_loop) {
		global_event_loop->removeEvent(this, event);
	}
}
