/* Kiwi handle class
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
 * @brief		Handle class.
 */

#ifndef __KIWI_HANDLE_H
#define __KIWI_HANDLE_H

#include <kernel/types.h>

#include <kiwi/internal.h>

namespace kiwi {

/** Base class for all objects represented by a handle. */
class Handle : internal::Noncopyable {
public:
	/** Destructor to close the handle. */
	~Handle();

	/** Wait for a handle event.
	 * @note		Derived classes should implement their own
	 *			functions to wait for events on top of this
	 *			function, which should be used rather than
	 *			using this function directly.
	 * @param event		Event to wait for (specific to handle type).
	 * @param timeout	Timeout in microseconds. A value of 0 will
	 *			return an error immediately if the event has
	 *			not already happened, and a value of -1 (the
	 *			default) will block indefinitely until the
	 *			event happens.
	 * @return		0 on success, error code on failure. */
	int Wait(int event, timeout_t timeout = -1) const;

	/** Get the ID of the handle.
	 * @return		ID of the handle. */
	handle_t GetHandleID(void) const;
protected:
	/** Handle class cannot be instantiated directly. */
	Handle();

	handle_t m_handle;		/**< Handle ID for the process. */
};

};

#endif /* __KIWI_HANDLE_H */
