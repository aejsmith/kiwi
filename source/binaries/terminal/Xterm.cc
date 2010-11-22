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
 * @brief		Xterm emulator class.
 *
 * Reference:
 * - ANSI/VT100 Terminal Control
 *   http://www.termsys.demon.co.uk/vtansi.htm
 * - Wikipedia: ANSI escape code
 *   http://en.wikipedia.org/wiki/ANSI_escape_code
 * - ASCII Table - ANSI Escape Sequences
 *   http://ascii-table.com/ansi-escape-sequences.php
 * - Xterm Control Sequences
 *   http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 */

#include "Xterm.h"

/** Initialise the Xterm emulator.
 * @param window	Window to draw to. */
Xterm::Xterm(TerminalWindow *window) :
	m_window(window), m_active_buffer(0)
{
	/* The buffers will be created when this handler is added to a terminal
	 * in the Resize() function. */
	m_buffers[0] = 0;
	m_buffers[1] = 0;
}

/** Destroy the Xterm object. */
Xterm::~Xterm() {
	if(m_buffers[0]) {
		delete m_buffers[0];
	}
	if(m_buffers[1]) {
		delete m_buffers[1];
	}
}

/** Resize the terminal.
 * @param cols		New width of the terminal.
 * @param rows		New height of the terminal. */
void Xterm::Resize(int cols, int rows) {
	if(m_buffers[0]) {
		m_buffers[0]->Resize(cols, rows);
	} else {
		m_buffers[0] = new TerminalBuffer(m_window, cols, rows, true);
	}
	if(m_buffers[1]) {
		m_buffers[1]->Resize(cols, rows);
	} else {
		m_buffers[1] = new TerminalBuffer(m_window, cols, rows, false);
	}
}

/** Write output to the terminal.
 * @param raw		Character to output. */
void Xterm::Output(unsigned char raw) {
	TerminalBuffer::Character ch = {
		raw,
		TerminalBuffer::kDefaultColour,
		TerminalBuffer::kDefaultColour,
		false
	};
	m_buffers[m_active_buffer]->Output(ch);
}

/** Get the active buffer.
 * @return		Active buffer. */
TerminalBuffer *Xterm::GetBuffer() {
	return m_buffers[m_active_buffer];
}
