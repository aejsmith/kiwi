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
	class Handle : public internal::Noncopyable {
	public:
		~Handle();

		int Wait(int event, timeout_t timeout = -1) const;
		handle_t GetHandle(void) const;
	protected:
		Handle();

		handle_t m_handle;		/**< Handle ID. */
	};
};

#endif /* __KIWI_HANDLE_H */
