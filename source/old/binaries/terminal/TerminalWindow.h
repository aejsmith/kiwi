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
 * @brief		Terminal window class.
 */

#ifndef __TERMINALWINDOW_H
#define __TERMINALWINDOW_H

#include <kiwi/Graphics/BaseWindow.h>

#include "Font.h"
#include "TerminalApp.h"
#include "Terminal.h"
#include "Xterm.h"

/** Class for a terminal window. */
class TerminalWindow : public kiwi::BaseWindow {
public:
	TerminalWindow(TerminalApp *app, int cols, int rows);

	/** Get the window's terminal.
	 * @return		Reference to the window's terminal. */
	Terminal &GetTerminal() { return m_terminal; }

	void TerminalUpdated(kiwi::Rect rect);
	void TerminalScrolled(int start, int end, int delta);
	void TerminalHistoryAdded();
	void TerminalBufferChanged();
	void Flush();
private:
	void TerminalExited(int status);
	void ScrollUp(int amount);
	void ScrollDown(int amount);
	void DoScroll(int start, int end, int delta);
	void SendInput(unsigned char ch);

	void KeyPressed(const kiwi::KeyEvent &event);
	void Resized(const kiwi::ResizeEvent &event);

	TerminalApp *m_app;		/**< Application the window is on. */
	Xterm m_xterm;			/**< Xterm emulator. */
	Terminal m_terminal;		/**< Terminal device for the window. */
	int m_cols;			/**< Width of the terminal. */
	int m_rows;			/**< Height of the terminal. */
	int m_history_pos;		/**< Offset in history. */
	kiwi::Region m_updated;		/**< Updated region. */

	static Font *m_font;		/**< Normal font to use. */
	static Font *m_bold_font;	/**< Bold font to use. */
};

#endif /* __TERMINALWINDOW_H */
