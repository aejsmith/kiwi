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
 * @brief		Base input device class.
 */

#ifndef __INPUTDEVICE_H
#define __INPUTDEVICE_H

#include <kiwi/Handle.h>

#include "InputManager.h"

/** Base class for an input device. */
class InputDevice : public kiwi::Handle {
public:
	InputDevice(InputManager *manager, handle_t handle);
protected:
	virtual void HandleEvent(input_event_t &event) = 0;

	InputManager *m_manager;	/**< Input manager the device is for. */
private:
	void RegisterEvents();
	void HandleEvent(int id);
};

#endif /* __INPUTDEVICE_H */
