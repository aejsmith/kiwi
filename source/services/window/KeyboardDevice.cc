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
 * @todo		Num Lock.
 */

#include <drivers/input.h>

#include <ctype.h>
#include <iostream>

#include "KeyboardDevice.h"

using namespace kiwi;
using namespace std;

extern unsigned char key_table[];
extern unsigned char key_table_shift[];
extern unsigned char key_table_ctrl[];

/** Initialise the keyboard device.
 * @param manager	Manager of the device.
 * @param handle	Handle to device. */
KeyboardDevice::KeyboardDevice(InputManager *manager, handle_t handle) :
	InputDevice(manager, handle)
{}

/** Handle an event.
 * @param event		Kernel event structure. */
void KeyboardDevice::HandleEvent(input_event_t &event) {
	unsigned char *table;

	/* Get the text for the key. */
	string text;
	table = (m_manager->GetModifiers() & Input::kShiftModifier) ? key_table_shift : key_table;
	if(m_manager->GetModifiers() & Input::kControlModifier && key_table_ctrl[event.value]) {
		text += key_table_ctrl[event.value];
	} else if(table[event.value]) {
		if(m_manager->GetModifiers() & Input::kCapsLockModifier && isalpha(table[event.value])) {
			text += toupper(table[event.value]);
		} else {
			text += table[event.value];
		}
	}

	switch(event.type) {
	case INPUT_EVENT_KEY_DOWN:
		m_manager->KeyPress(event.time, event.value, text);
		break;
	case INPUT_EVENT_KEY_UP:
		m_manager->KeyRelease(event.time, event.value, text);
		break;
	default:
		clog << "Unrecognised keyboard event: " << event.type << endl;
		break;
	}
}
