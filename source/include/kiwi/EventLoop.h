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
 * @brief		Kiwi event loop class.
 */

#ifndef __KIWI_EVENTLOOP_H
#define __KIWI_EVENTLOOP_H

#include <kiwi/Handle.h>

KIWI_BEGIN_NAMESPACE

class EventLoopPrivate;

/** Class implementing a loop for handling object events. */
class KIWI_PUBLIC EventLoop : public Object, Noncopyable {
	friend class Object;
public:
	EventLoop();
	~EventLoop();

	void AddEvent(Handle *handle, int event);
	void RemoveEvent(Handle *handle, int event);
	void RemoveHandle(Handle *handle);
	void Run();

	static EventLoop *Instance();
private:
	KIWI_PRIVATE void DeleteObject(Object *obj);

	EventLoopPrivate *m_priv;	/**< Internal data pointer. */
};

KIWI_END_NAMESPACE

#endif /* __KIWI_EVENTLOOP_H */
