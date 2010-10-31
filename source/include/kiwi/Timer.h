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

	/** Signal emitted when the timer event fires. */
	Signal<> OnTimer;
private:
	void RegisterEvents();
	void HandleEvent(int event);

	Mode m_mode;			/**< Timer mode. */
};

}

#endif /* __KIWI_TIMER_H */
