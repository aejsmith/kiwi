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
 * @brief		Mouse device class.
 */

#ifndef __MOUSEDEVICE_H
#define __MOUSEDEVICE_H

#include <kiwi/Handle.h>

class WindowServer;

/** Class representing a mouse device. */
class MouseDevice : public kiwi::Handle {
public:
	MouseDevice(WindowServer *server, handle_t handle);
private:
	void RegisterEvents();
	void EventReceived(int id);

	WindowServer *m_server;		/**< Server that the display is for. */
};

#endif /* __MOUSEDEVICE_H */