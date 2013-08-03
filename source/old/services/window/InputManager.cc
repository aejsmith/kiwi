/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Input device manager.
 *
 * This class watches the input device directory for new devices and creates
 * devices for them.
 *
 * @todo		The kernel doesn't actually have any facilities to
 *			watch a device directory yet. It also doesn't let us
 *			get attributes.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <iostream>

#include "Cursor.h"
#include "InputManager.h"
#include "KeyboardDevice.h"
#include "MouseDevice.h"
#include "Session.h"
#include "WindowServer.h"

using namespace kiwi;
using namespace std;

/** Initialise the input manager.
 * @param server	Server that the manager is for. */
InputManager::InputManager(WindowServer *server) :
	m_server(server), m_modifiers(0), m_buttons(0)
{
	handle_t handle;
	status_t ret;

	/* See above TODO. Just hardcode devices for now. */
	ret = kern_device_open("/input/0", DEVICE_RIGHT_READ | DEVICE_RIGHT_WRITE, &handle);
	if(ret == STATUS_SUCCESS) {
		new KeyboardDevice(this, handle);
	} else {
		clog << "Failed to open /input/0: " << ret << endl;
	}
	ret = kern_device_open("/input/1", DEVICE_RIGHT_READ | DEVICE_RIGHT_WRITE, &handle);
	if(ret == STATUS_SUCCESS) {
		new MouseDevice(this, handle);
	} else {
		clog << "Failed to open /input/1: " << ret << endl;
	}
}

/** Handle a mouse move event.
 * @param time		Time of the event.
 * @param dx		X position delta.
 * @param dy		Y position delta. */
void InputManager::MouseMove(useconds_t time, int dx, int dy) {
	m_server->GetActiveSession()->MouseMoved(time, dx, dy, m_modifiers, m_buttons);
}

/** Convert a kernel button code.
 * @param button	Kernel button code.
 * @return		Mask for the button. */
static inline uint32_t convert_button(int32_t button) {
	switch(button) {
	case INPUT_BUTTON_LEFT:
		return Input::kLeftButton;
	case INPUT_BUTTON_RIGHT:
		return Input::kRightButton;
	case INPUT_BUTTON_MIDDLE:
		return Input::kMiddleButton;
	default:
		return 0;
	}
}

/** Handle a mouse press event.
 * @param time		Time of the event.
 * @param button	Kernel code for button that was pressed. */
void InputManager::MousePress(useconds_t time, int32_t button) {
	m_buttons |= convert_button(button);
	m_server->GetActiveSession()->MousePressed(time, m_modifiers, m_buttons);
}

/** Handle a mouse release event.
 * @param time		Time of the event.
 * @param button	Kernel code for button that was released. */
void InputManager::MouseRelease(useconds_t time, int32_t button) {
	m_buttons &= ~convert_button(button);
	m_server->GetActiveSession()->MouseReleased(time, m_modifiers, m_buttons);
}

/** Convert a kernel key code to a modifier.
 * @param key		Kernel key code.
 * @return		Mask for the modifier, or 0 if not a modifier. */
static inline uint32_t convert_modifier(int32_t key) {
	switch(key) {
	case INPUT_KEY_LCTRL:
	case INPUT_KEY_RCTRL:
		return Input::kControlModifier;
	case INPUT_KEY_LALT:
	case INPUT_KEY_RALT:
		return Input::kAltModifier;
	case INPUT_KEY_LSUPER:
	case INPUT_KEY_RSUPER:
		return Input::kSuperModifier;
	case INPUT_KEY_LSHIFT:
	case INPUT_KEY_RSHIFT:
		return Input::kShiftModifier;
	default:
		return 0;
	}
}

/** Convert a kernel key code to a lock.
 * @param key		Kernel key code.
 * @return		Mask for the lock, or 0 if not a lock. */
static inline uint32_t convert_lock(int32_t key) {
	switch(key) {
	case INPUT_KEY_CAPSLOCK:
		return Input::kCapsLockModifier;
	case INPUT_KEY_SCROLLLOCK:
		return Input::kScrollLockModifier;
	case INPUT_KEY_NUMLOCK:
		return Input::kNumLockModifier;
	default:
		return 0;
	}
}

/** Handle a key press event.
 * @param time		Time of the event.
 * @param key		Kernel code for key that was pressed.
 * @param text		Text generated by the key. */
void InputManager::KeyPress(useconds_t time, int32_t key, const std::string &text) {
	/* If the key is a modifier, enable it. */
	m_modifiers |= convert_modifier(key);

	/* If it's a lock key, toggle the state. */
	uint32_t lock = convert_lock(key);
	if(lock) {
		m_modifiers ^= lock;
	}

	/* Send the event to the session. */
	KeyEvent event(Event::kKeyPress, time, m_modifiers, key, text);
	m_server->GetActiveSession()->KeyPressed(event);
}

/** Handle a key release event.
 * @param time		Time of the event.
 * @param key		Kernel code for key that was released.
 * @param text		Text generated by the key. */
void InputManager::KeyRelease(useconds_t time, int32_t key, const std::string &text) {
	/* If the key is a modifier, disable it. */
	m_modifiers &= ~convert_modifier(key);

	/* Send the event to the session. */
	KeyEvent event(Event::kKeyRelease, time, m_modifiers, key, text);
	m_server->GetActiveSession()->KeyReleased(event);
}