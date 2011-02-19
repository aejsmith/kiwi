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
 * @brief		Terminal emulator.
 */

#ifndef __TERMINALAPP_H
#define __TERMINALAPP_H

#include <kiwi/EventLoop.h>
#include <list>

class TerminalWindow;

/** Terminal application class. */
class TerminalApp : public kiwi::EventLoop {
public:
	TerminalApp(int argc, char **argv);

	void CreateWindow();
private:
	void PostHandle();
	void WindowDestroyed(Object *obj);

	/** List of all windows. */
	std::list<TerminalWindow *> m_windows;
};

#endif /* __TERMINALAPP_H */
