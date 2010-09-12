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
#include <kernel/status.h>

#include <kiwi/EventLoop.h>
#include <kiwi/Handle.h>
#include <kiwi/private/log.h>

using namespace kiwi;

/** Handle constructor.
 * @note		ARGH RAGE!!! When a base class constructor is run the
 *			object acts as though it's an instance of the base
 *			class, not the derived class. That means that the
 *			object's vtable will point to Handle's, not the derived
 *			class's. Because of this, calling setHandle here will
 *			not register events because the registerEvents that
 *			will be used is Handle's, not the derived class's. So,
 *			we must do the setHandle in derived class' constructors.
 *			http://www.artima.com/cppsource/nevercall.html
 *			http://www.artima.com/cppsource/pure_virtual.html */
Handle::Handle() : m_handle(-1) {}

/** Destructor to close the handle. */
Handle::~Handle() {
	Close();
}

/** Close the handle. */
void Handle::Close() {
	if(m_handle >= 0) {
		/* Remove all events for this handle from the event loop. */
		EventLoop *loop = EventLoop::Instance();
		if(loop) {
			loop->RemoveHandle(this);
		}

		/* Run callbacks. */
		OnClose();

		/* The only error handle_close() can encounter is the handle
		 * not existing. Therefore, if we fail, it means the programmer
		 * has done something funny so we raise a fatal error. */
		if(handle_close(m_handle) != STATUS_SUCCESS) {
			log::fatal("Handle::Close: Handle %d has already been closed.\n", m_handle);
		}

		m_handle = -1;
	}
}

/** Get the kernel handle the handle is using.
 * @return		Raw handle. Do not close this. */
handle_t Handle::GetHandle() const {
	return m_handle;
}

/** Wait for an event on the object referred to by the handle.
 * @note		This is protected as derived classes should implement
 *			their own functions to wait for events on top of this
 *			function.
 * @param event		Event to wait for (specific to handle type).
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the event has not already
 *			happened, and a value of -1 (the default) will block
 *			indefinitely until the event happens.
 * @return		True on success, false if the operation timed out. */
bool Handle::Wait(int event, useconds_t timeout) const {
	status_t ret = object_wait(m_handle, event, timeout);
	if(ret == STATUS_SUCCESS) {
		return true;
	}

	/* Handle errors that can occur because the programmer has done
	 * something daft. */
	if(ret == STATUS_INVALID_HANDLE) {
		log::fatal("Handle::Wait: Handle %d is invalid.\n", m_handle);
	} else if(ret == STATUS_INVALID_EVENT) {
		log::fatal("Handle::Wait: Event %d is invalid for handle %d\n", event, m_handle);
	}

	return false;
}

/** Set the kernel handle to use.
 * @param handle	New handle. The current handle (if any) will be closed. */
void Handle::SetHandle(handle_t handle) {
	Close();
	m_handle = handle;
	if(m_handle >= 0) {
		RegisterEvents();
	}
}

/** Register an event with the current thread's event loop.
 * @param event		Event ID to register. */
void Handle::RegisterEvent(int event) {
	EventLoop *loop = EventLoop::Instance();
	if(loop) {
		loop->AddEvent(this, event);
	}
}

/** Unregister an event with the current thread's event loop.
 * @param event		Event ID to unregister. */
void Handle::UnregisterEvent(int event) {
	EventLoop *loop = EventLoop::Instance();
	if(loop) {
		loop->RemoveEvent(this, event);
	}
}

/** Register all events that the event loop should poll for. */
void Handle::RegisterEvents() {}

/** Handle an event received for the handle. */
void Handle::EventReceived(int id) {}
