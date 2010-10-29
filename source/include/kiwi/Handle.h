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

#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Object.h>

KIWI_BEGIN_NAMESPACE

class EventLoop;

/** Base class for all objects accessed through a handle. */
class KIWI_PUBLIC Handle : public Object, Noncopyable {
	friend class EventLoop;
public:
	virtual ~Handle();

	void Close();
	void InhibitEvents(bool inhibit);

	/** Get the kernel handle for this object.
	 * @return		Kernel handle, or -1 if not currently open. Do
	 *			NOT close the returned handle. */
	handle_t GetHandle() const { return m_handle; }

	/** Signal emitted when the handle is closed. */
	Signal<> OnClose;
protected:
	Handle();

	void SetHandle(handle_t handle);
	void RegisterEvent(int event);
	void UnregisterEvent(int event);
	status_t _Wait(int event, useconds_t timeout) const;

	virtual void RegisterEvents();
	virtual void HandleEvent(int event);

	handle_t m_handle;		/**< Handle ID. */
	EventLoop *m_event_loop;	/**< Event loop handling this handle. */
};

KIWI_END_NAMESPACE

#endif /* __KIWI_HANDLE_H */
