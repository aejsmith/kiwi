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
 * @brief		Mouse receiver interface.
 */

#ifndef __MOUSERECEIVER_H
#define __MOUSERECEIVER_H

#include <kiwi/Graphics/InputEvent.h>
#include <kiwi/Graphics/Point.h>

/** Interface class for objects that can receive mouse events. */
class MouseReceiver {
public:
	virtual void MouseMoved(const kiwi::MouseEvent &event) = 0;
	virtual void MousePressed(const kiwi::MouseEvent &event) = 0;
	virtual void MouseReleased(const kiwi::MouseEvent &event) = 0;
	virtual kiwi::Point RelativePoint(const kiwi::Point &pos) const = 0;
};

#endif /* __MOUSERECEIVER_H */
