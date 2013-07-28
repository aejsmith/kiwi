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
