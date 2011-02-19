/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Handle class.
 */

#ifndef __KIWI_HANDLE_H
#define __KIWI_HANDLE_H

#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Error.h>
#include <kiwi/Object.h>

namespace kiwi {

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
private:
	EventLoop *m_event_loop;	/**< Event loop handling this handle. */
};

/** Base handle class with an Error object.
 * @note		See documentation for Error for when to use this. */
class KIWI_PUBLIC ErrorHandle : public Handle {
public:
	/** Get information about the last error that occurred.
	 * @return		Reference to error object for last error. */
	const Error &GetError() const { return m_error; }
protected:
	/** Set the error information.
	 * @param code		Status code to set. */
	void SetError(status_t code) { m_error = code; }

	/** Set the error information.
	 * @param error		Error object to set. */
	void SetError(const Error &error) { m_error = error.GetCode(); }
private:
	Error m_error;			/**< Error information. */
};

}

#endif /* __KIWI_HANDLE_H */
