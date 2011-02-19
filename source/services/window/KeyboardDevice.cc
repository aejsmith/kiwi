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
