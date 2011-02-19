/*
 * Copyright (C) 2010 Alex Smith
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
class KIWI_PUBLIC Thread : public ErrorHandle {
public:
	Thread(handle_t handle = -1);
	~Thread();

	bool Open(thread_id_t id);
	void SetName(const char *name);
	bool Run();
	bool Wait(useconds_t timeout = -1) const;
	void Quit(int status = 0);

	bool IsRunning() const;
	int GetStatus() const;
	thread_id_t GetID() const;

	/** Signal emitted when the thread exits.
	 * @param		Exit status code. */
	Signal<int> OnExit;

	static thread_id_t GetCurrentID();
	static void Sleep(useconds_t usecs);
protected:
	EventLoop &GetEventLoop();
	virtual int Main();
private:
	void RegisterEvents();
	void HandleEvent(int event);

	KIWI_PRIVATE static void _Entry(void *arg);

	ThreadPrivate *m_priv;		/**< Internal data pointer. */
};

}

#endif /* __KIWI_THREAD_H */
