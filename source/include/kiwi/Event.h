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
 * @brief		Event class.
 */

#ifndef __KIWI_EVENT_H
#define __KIWI_EVENT_H

#include <kiwi/CoreDefs.h>

namespace kiwi {

/** Base class for all events that can occur in an application. */
class KIWI_PUBLIC Event {
public:
	/** Event type IDs. */
	enum Type {
		kMouseMove,		/**< Mouse movement. */
		kMousePress,		/**< Mouse button pressed. */
		kMouseRelease,		/**< Mouse button released. */
		kKeyPress,		/**< Key press. */
		kKeyRelease,		/**< Key release. */
		kResize,		/**< Window/widget resized. */
		kWindowCreate,		/**< Child window created. */
		kWindowDestroy,		/**< Window destroyed. */
		kWindowShow,		/**< Window shown. */
		kWindowHide,		/**< Window hidden. */
		kWindowActivate,	/**< Window activated. */
		kWindowDeactivate,	/**< Window deactivate. */
		kWindowTitleChange,	/**< Window title changed. */
		kWindowStateChange,	/**< Window state has changed. */
	};

	/** Initialise the event.
	 * @param type		Type of the event. */
	Event(Type type) : m_type(type) {}

	/** Get the type of the event.
	 * @return		Type of the event. */
	Type GetType() const { return m_type; }
private:
	Type m_type;			/**< Type of the event. */
};

}

#endif /* __KIWI_EVENT_H */
