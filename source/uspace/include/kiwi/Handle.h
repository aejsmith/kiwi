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

#ifndef __KIWI_HANDLE_H
#define __KIWI_HANDLE_H

#include <kernel/types.h>

#include <kiwi/Object.h>
#include <kiwi/Signal.h>

namespace kiwi {

class EventLoop;

/** Base class for all objects accessed through a handle. */
class Handle : public Object, internal::Noncopyable {
	friend class EventLoop;
public:
	virtual ~Handle();

	virtual bool close();
	bool wait(int event, useconds_t timeout = -1) const;
	handle_t getHandle(void) const;

	Signal<Handle *> onClose;
protected:
	Handle(handle_t handle = -1);

	void registerEvent(int event);
	void unregisterEvent(int event);
	virtual void eventReceived(int id) = 0;

	handle_t m_handle;		/**< Handle ID. */
};

}

#endif /* __KIWI_HANDLE_H */
