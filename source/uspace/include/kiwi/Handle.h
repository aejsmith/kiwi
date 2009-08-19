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

KIWI_BEGIN_NAMESPACE

/** Base class for all objects represented by a handle. */
class Handle : internal::Noncopyable {
public:
	/** Destructor to close the handle. */
	~Handle();

	/** Get the ID of this handle.
	 * @return		ID of the handle. */
	handle_t get_handle_id(void) {
		return m_handle;
	}
protected:
	/** Handle class cannot be instantiated directly. */
	Handle() : m_handle(-1) {}

	handle_t m_handle;		/**< Handle ID for the process. */
};

KIWI_END_NAMESPACE

#endif /* __KIWI_HANDLE_H */
