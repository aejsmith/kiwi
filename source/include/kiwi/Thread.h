/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Thread class.
 */

#ifndef __KIWI_THREAD_H
#define __KIWI_THREAD_H

#include <kiwi/Error.h>
#include <kiwi/EventLoop.h>
#include <kiwi/Handle.h>

namespace kiwi {

struct ThreadPrivate;

/** Class implementing a thread. */
class KIWI_PUBLIC Thread : public Handle {
public:
	Thread();
	Thread(handle_t handle);
	~Thread();

	bool Open(thread_id_t id);
	void SetName(const char *name);
	bool Run();
	bool Wait(useconds_t timeout = -1) const;

	bool IsRunning() const;
	int GetStatus() const;
	thread_id_t GetID() const;

	/** Get information about the last error that occurred.
	 * @return		Reference to error object for last error. */
	const Error &GetError() const { return m_error; }

	/** Signal emitted when the thread exits.
	 * @param		Exit status code. */
	Signal<int> OnExit;

	static thread_id_t GetCurrentID();
	static void Sleep(useconds_t usecs);
protected:
	/** Get the thread's event loop.
	 * @return		Reference to thread's event loop. */
	EventLoop &GetEventLoop() { return *m_event_loop; }

	virtual int Main();
private:
	void RegisterEvents();
	void HandleEvent(int id);

	KIWI_PRIVATE static void _Entry(void *arg);

	Error m_error;			/**< Error information. */
	EventLoop *m_event_loop;	/**< Event loop for the thread. */
	ThreadPrivate *m_priv;		/**< Internal data pointer. */
};

}

#endif /* __KIWI_THREAD_H */
