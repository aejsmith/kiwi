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
 * @brief		Timer class.
 */

#ifndef __KIWI_TIMER_H
#define __KIWI_TIMER_H

#include <kiwi/Handle.h>

namespace kiwi {

/** Class implementing an timer. */
class KIWI_PUBLIC Timer : public Handle {
public:
	/** Timer mode values. */
	enum Mode {
		kOneShotMode,		/**< Fire the timer once. */
		kPeriodicMode,		/**< Fire the timer periodically. */
	};

	Timer(Mode mode);

	void Start(useconds_t interval);
	void Stop();

	/** Check whether the timer is running.
	 * @return		Whether the timer is running. */
	bool IsRunning() const { return m_running; }

	/** Signal emitted when the timer event fires. */
	Signal<> OnTimer;
private:
	void RegisterEvents();
	void HandleEvent(int event);

	Mode m_mode;			/**< Timer mode. */
	bool m_running;			/**< Whether the timer is running. */
};

}

#endif /* __KIWI_TIMER_H */
