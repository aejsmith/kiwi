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
 * @brief		Window class.
 */

#ifndef __KIWI_UI_WINDOW_H
#define __KIWI_UI_WINDOW_H

#include <kiwi/Object.h>

namespace kiwi {

/** UI window class. */
class Window : public Object {
public:
	/** Type of a window ID. */
	typedef int32_t ID;

	/** Window event types. */
	enum EventType {
		kCreateEvent = (1<<0),		/**< A child window is created. */
		kDestroyEvent = (1<<1),		/**< Window is destroyed. */
		kCloseEvent = (1<<2),		/**< Request for the window to close. */
		kRenameEvent = (1<<3),		/**< Window title is changed. */
		kMoveEvent = (1<<4),		/**< Window is moved. */
		kResizeEvent = (1<<5),		/**< Raised when window is resized. */
		kUpdateEvent = (1<<6),		/**< An area of the window's surface is updated. */
	};

	Window();
	Window(ID id);

	
private:
	ID m_id;				/**< ID of the window. */
};

}

#endif /* __KIWI_UI_WINDOW_H */
