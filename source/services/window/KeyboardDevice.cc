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
 * @brief		Keyboard device class.
 *
 * @todo		Keymap support.
 */

#include <drivers/input.h>
#include <iostream>
#include "KeyboardDevice.h"

using namespace std;

/** Initialise the keyboard device.
 * @param manager	Manager of the device.
 * @param handle	Handle to device. */
KeyboardDevice::KeyboardDevice(InputManager *manager, handle_t handle) :
	InputDevice(manager, handle)
{}

/** Handle an event.
 * @param event		Kernel event structure. */
void KeyboardDevice::HandleEvent(input_event_t &event) {
	switch(event.type) {
	case INPUT_EVENT_KEY_DOWN:
		m_manager->KeyPress(event.time, event.value, string());
		break;
	case INPUT_EVENT_KEY_UP:
		m_manager->KeyRelease(event.time, event.value, string());
		break;
	default:
		clog << "Unrecognised keyboard event: " << event.type << endl;
		break;
	}
}
