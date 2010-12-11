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

#include "Internal.h"

using namespace kiwi;

/** Handle constructor.
 * @note		When a base class constructor is run the object acts as
 *			though it's an instance of the base class, not the
 *			derived class. That means that the object's vtable will
 *			point to Handle's, not the derived class'. Because of
 *			this, calling SetHandle() here will not register events
 *			because the RegisterEvents() that will be used is
 *			Handle's, not the derived class's. So, we must do the
 *			SetHandle() call in derived class' constructors.
 *			http://www.artima.com/cppsource/nevercall.html
 *			http://www.artima.com/cppsource/pure_virtual.html */
Handle::Handle() : m_handle(-1), m_event_loop(EventLoop::Instance()) {}

/** Destructor to close the handle. */
Handle::~Handle() {
	Close();
}

/** Close the handle. */
void Handle::Close() {
	if(m_handle >= 0) {
		/* Remove this handle from the event loop. */
		if(m_event_loop) {
			m_event_loop->DetachHandle(this);
		}

		/* Emit the close event. */
		OnClose();

		/* The only error kern_handle_close() can encounter is the
		 * handle not existing. Therefore, if we fail, it means the
		 * programmer has done something funny so we raise a fatal
		 * error. */
		if(unlikely(kern_handle_close(m_handle) != STATUS_SUCCESS)) {
			libkiwi_fatal("Handle::Close: Handle %d has already been closed.", m_handle);
		}

		m_handle = -1;
	}
}

/** Set whether events from the handle are inhibited.
 * @note		When the handle is changed to refer to a different
 *			object, events will be re-enabled.
 * @param inhbit	Whether to inhibit events. */
void Handle::InhibitEvents(bool inhibit) {
	if(m_handle >= 0 && m_event_loop) {
		m_event_loop->RemoveEvents(this);
		if(!inhibit) {
			RegisterEvents();
		}
	}
}

/** Wait for an event on the object referred to by the handle.
 * @note		This is protected as derived classes should implement
 *			their own functions to wait for events on top of this
 *			function.
 * @param event		Event to wait for (specific to object type).
 * @param timeout	Timeout in microseconds. A value of 0 will return an
 *			error immediately if the event has not already happened,
 *			and a value of -1 will block indefinitely until the
 *			event happens.
 * @return		True on success, false if the operation timed out. */
status_t Handle::_Wait(int event, useconds_t timeout) const {
	object_event_t _event = { m_handle, event, false };
	status_t ret;

	ret = kern_object_wait(&_event, 1, timeout);
	if(unlikely(ret != STATUS_SUCCESS)) {
		/* Handle errors that can occur because the programmer has done
		 * something daft. */
		if(ret == STATUS_INVALID_HANDLE) {
			libkiwi_fatal("Handle::Wait: Handle %d is invalid.", m_handle);
		} else if(ret == STATUS_INVALID_EVENT) {
			libkiwi_fatal("Handle::Wait: Event %d is invalid for handle %d.",
			              event, m_handle);
		}
	}

	return ret;
}

/** Set the kernel handle to use.
 * @param handle	New handle. The current handle (if any) will be closed. */
void Handle::SetHandle(handle_t handle) {
	Close();
	m_handle = handle;

	/* Attach the handle to the event loop. */
	if(m_handle >= 0 && m_event_loop) {
		m_event_loop->AttachHandle(this);
		RegisterEvents();
	}
}

/** Register an event with the current thread's event loop.
 * @param event		Event ID to register. */
void Handle::RegisterEvent(int event) {
	if(m_event_loop) {
		m_event_loop->AddEvent(this, event);
	}
}

/** Unregister an event with the current thread's event loop.
 * @param event		Event ID to unregister. */
void Handle::UnregisterEvent(int event) {
	if(m_event_loop) {
		m_event_loop->RemoveEvent(this, event);
	}
}

/** Register all events that the event loop should poll for. */
void Handle::RegisterEvents() {}

/** Handle an event received for the handle.
 * @param event		ID of event. */
void Handle::HandleEvent(int event) {}
