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
 * @brief		Input device manager.
 */

#ifndef __INPUTMANAGER_H
#define __INPUTMANAGER_H

#include <kiwi/Graphics/InputEvent.h>
#include <kiwi/Support/Noncopyable.h>

class WindowServer;

/** Class managing input devices. */
class InputManager : kiwi::Noncopyable {
public:
	InputManager(WindowServer *server);

	void MouseMove(useconds_t time, int dx, int dy);
	void MousePress(useconds_t time, int32_t button);
	void MouseRelease(useconds_t time, int32_t button);
	void KeyPress(useconds_t time, int32_t key, const std::string &text);
	void KeyRelease(useconds_t time, int32_t key, const std::string &text);

	/** Get the modifier state.
	 * @return		Modifier state. */
	uint32_t GetModifiers() const { return m_modifiers; }
private:
	WindowServer *m_server;		/**< Server that the manager is for. */
	uint32_t m_modifiers;		/**< Currently pressed keyboard modifiers. */
	uint32_t m_buttons;		/**< Currently pressed mouse buttons. */
};

#endif /* __INPUTMANAGER_H */
