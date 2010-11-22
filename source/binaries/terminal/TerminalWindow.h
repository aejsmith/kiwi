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

	static Font *m_font;		/**< Normal font to use. */
	static Font *m_bold_font;	/**< Bold font to use. */
};

#endif /* __TERMINALWINDOW_H */
