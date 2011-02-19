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
 * @brief		Mouse device class.
 */

#include <drivers/input.h>
#include <iostream>
#include "MouseDevice.h"

using namespace std;

/** Initialise the mouse device.
 * @param manager	Manager of the device.
 * @param handle	Handle to device. */
MouseDevice::MouseDevice(InputManager *manager, handle_t handle) :
	InputDevice(manager, handle)
{}

/** Handle an event.
 * @param event		Kernel event structure. */
void MouseDevice::HandleEvent(input_event_t &event) {
	switch(event.type) {
	case INPUT_EVENT_REL_X:
		m_manager->MouseMove(event.time, event.value, 0);
		break;
	case INPUT_EVENT_REL_Y:
		m_manager->MouseMove(event.time, 0, event.value);
		break;
	case INPUT_EVENT_BTN_DOWN:
		m_manager->MousePress(event.time, event.value);
		break;
	case INPUT_EVENT_BTN_UP:
		m_manager->MouseRelease(event.time, event.value);
		break;
	default:
		clog << "Unrecognised mouse event: " << event.type << endl;
		break;
	}
}
