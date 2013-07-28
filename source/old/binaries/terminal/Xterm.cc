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

#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <string.h>

#include "TerminalWindow.h"
#include "Xterm.h"

using namespace std;

/** Initialise the Xterm emulator.
 * @param window	Window to draw to. */
Xterm::Xterm(TerminalWindow *window) :
	m_window(window), m_active_buffer(0), m_esc_state(0),
	m_esc_param_size(0), m_saved_x(0), m_saved_y(0)
{
	/* The buffers will be created when this handler is added to a terminal
	 * in the Resize() function. */
	m_buffers[0] = 0;
	m_buffers[1] = 0;

	/* Initialise default attributes. */
	m_attrib.fg = TerminalBuffer::kDefaultColour;
	m_attrib.bg = TerminalBuffer::kDefaultColour;
	m_attrib.bold = false;
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
 * @todo		The parameter collection code is duplicated in a few
 *			places. It shouldn't be.
 * @param raw		Character to output. */
void Xterm::Output(unsigned char raw) {
	int x, y, cols, rows;

	/* Get information used by multiple functions. */
	m_buffers[m_active_buffer]->GetCursor(x, y);
	m_buffers[m_active_buffer]->GetSize(cols, rows);

	switch(m_esc_state) {
	case 0:
		/* Check for escape characters. */
		if(raw == 0x1B) {
			/* Initially set the parameter size to -1, this allows
			 * functions which have a non-zero default value to see
			 * if they need to use it. */
			m_esc_param_size = -1;
			memset(m_esc_params, 0, sizeof(m_esc_params));
			m_esc_state = 1;
			return;
		}

		/* No escape and we're not currently parsing a code, must be a
		 * normal character. */
		{
			TerminalBuffer::Character ch = m_attrib;
			ch.ch = raw;
			m_buffers[m_active_buffer]->Output(ch);
		}
		return;
	case 1:
		switch(raw) {
		case '[':
			m_esc_state = 2;
			return;
		case ']':
			m_esc_state = 6;
			return;
		case '(':
		case ')':
			m_esc_state = 5;
			return;
		case '7':
			/* Save current cursor position. */
			m_buffers[m_active_buffer]->GetCursor(m_saved_x, m_saved_y);
			break;
		case '8':
			/* Restore saved cursor position. */
			m_buffers[m_active_buffer]->MoveCursor(m_saved_x, m_saved_y);
			break;
		case '=':
			/* Application Keypad. TODO. */
			break;
		case '>':
			/* Normal Keypad. TODO. */
			break;
		case 'D':
			m_buffers[m_active_buffer]->ScrollDown();
			break;
		case 'M':
			m_buffers[m_active_buffer]->ScrollUp();
			break;
		default:
			cerr << "Xterm::Output: Unknown character '" << raw << "' at state 1" << endl;
			break;
		}
		break;
	case 2:
		/* This state handles codes that do not take any arguments,
		 * if it encounters a digit or a code not handled by this
		 * state it falls through into state 3. */
		if(raw == 's') {
			/* Save current cursor position. */
			m_buffers[m_active_buffer]->GetCursor(m_saved_x, m_saved_y);
			break;
		} else if(raw == 'u') {
			/* Restore saved cursor position. */
			m_buffers[m_active_buffer]->MoveCursor(m_saved_x, m_saved_y);
			break;
		} else if(raw == '?') {
			m_esc_state = 4;
			return;
		}

		/* Character is a digit or something that takes arguments,
		 * fall through into state 3 to process numeric arguments. */
		m_esc_state = 3;
	case 3:
		/* Collect arguments for the function. Arguments are split
		 * with semi-colons. */
		if(isdigit(raw)) {
			if(m_esc_param_size < 0) {
				m_esc_param_size = 0;
			}
			m_esc_params[m_esc_param_size] *= 10;
			m_esc_params[m_esc_param_size] += (raw - '0');
			return;
		} else if(raw == ';') {
			if(++m_esc_param_size >= 8) {
				cerr << "Xterm::Output: Exceeded maximum number of parameters" << endl;
				break;
			}
			return;
		}

		/* Handle the code. */
		switch(raw) {
		case 'J':
			/* Erase in Display. */
			switch(m_esc_params[0]) {
			case 0:
				/* Erase Below. */
				m_buffers[m_active_buffer]->ClearLines(y, rows - 1);
				break;
			case 1:
				/* Erase Above. */
				m_buffers[m_active_buffer]->ClearLines(0, y);
				break;
			case 2:
				/* Erase All. */
				m_buffers[m_active_buffer]->ClearLines(0, rows - 1);
				break;
			}
			break;
		case 'K':
			/* Erase in Line. */
			switch(m_esc_params[0]) {
			case 0:
				/* Erase to Right. */
				m_buffers[m_active_buffer]->ClearLine(x, cols - 1);
				break;
			case 1:
				/* Erase to Left. */
				m_buffers[m_active_buffer]->ClearLine(0, x);
				break;
			case 2:
				/* Erase All. */
				m_buffers[m_active_buffer]->ClearLines(y, y);
				break;
			}
			break;
		case 'm':
			if(m_esc_param_size < 0) {
				m_esc_param_size = 0;
			}

			/* Character Attributes (SGR). */
			for(int i = 0; i <= m_esc_param_size; i++) {
				switch(m_esc_params[i]) {
				case 0:
					/* Reset attributes to defaults. */
					m_attrib.fg = TerminalBuffer::kDefaultColour;
					m_attrib.bg = TerminalBuffer::kDefaultColour;
					m_attrib.bold = false;
					break;
				case 1:
					/* Set bold colour. */
					m_attrib.bold = true;
					break;
				case 2:
					/* Set normal colour. */
					m_attrib.bold = false;
					break;
				case 7:
					/* Reverse colours. */
					swap(m_attrib.fg, m_attrib.bg);
					break;
				case 30 ... 37:
					/* Set foreground colour. */
					m_attrib.fg = static_cast<TerminalBuffer::Colour>(m_esc_params[i] - 30);
					break;
				case 39:
					/* Reset foreground to default. */
					m_attrib.fg = TerminalBuffer::kDefaultColour;
					break;
				case 40 ... 47:
					/* Set background colour. */
					m_attrib.fg = static_cast<TerminalBuffer::Colour>(m_esc_params[i] - 40);
					break;
				case 49:
					/* Reset background to default. */
					m_attrib.bg = TerminalBuffer::kDefaultColour;
					break;
				}
			}
			break;
		case 'H':
		case 'f':
			/* This is the right way round, the parameters go
			 * ESC[{row};{column}H. */
			if(m_esc_param_size < 1) {
				m_esc_params[1] = 1;
				if(m_esc_param_size < 0) {
					m_esc_params[0] = 1;
				}
			}
			m_buffers[m_active_buffer]->MoveCursor(m_esc_params[1] - 1, m_esc_params[0] - 1);
			break;
		case 'A':
			/* Cursor Up. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(x, y - m_esc_params[0]);
			break;
		case 'B':
			/* Cursor Down. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(x, y + m_esc_params[0]);
			break;
		case 'C':
			/* Cursor Forward. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(x + m_esc_params[0], y);
			break;
		case 'D':
			/* Cursor Backward. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(x - m_esc_params[0], y);
			break;
		case 'd':
			/* Line Position Absolute. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(x, m_esc_params[0] - 1);
			break;
		case 'G':
			/* Cursor Character Absolute. */
			if(m_esc_param_size < 0) {
				m_esc_params[0] = 1;
			}
			m_buffers[m_active_buffer]->MoveCursor(m_esc_params[0] - 1, y);
			break;
		case 'r':
			if(m_esc_param_size < 1) {
				m_esc_params[1] = rows;
				if(m_esc_param_size < 0) {
					m_esc_params[0] = 1;
				}
			}
			m_buffers[m_active_buffer]->SetScrollRegion(m_esc_params[0] - 1, m_esc_params[1] - 1);
			break;
		default:
			cerr << "Xterm::Output: Unknown character '" << raw << "' at state 3" << endl;
			break;
		}
		break;
	case 4:
		if(isdigit(raw)) {
			m_esc_params[m_esc_param_size] *= 10;
			m_esc_params[m_esc_param_size] += (raw - '0');
			return;
		}

		if(raw == 'h') {
			/* DEC Private Mode Set. */
			switch(m_esc_params[0]) {
			case 1049:
				/* Save Cursor and Use Alternative Screen Buffer. */
				m_buffers[m_active_buffer]->GetCursor(m_saved_x, m_saved_y);
			case 47:
			case 1047:
				/* Use Alternate Screen Buffer. */
				m_active_buffer = 1;
				m_window->TerminalBufferChanged();
				break;
			case 1048:
				/* Save Cursor. */
				m_buffers[m_active_buffer]->GetCursor(m_saved_x, m_saved_y);
				break;
			}
		} else if(raw == 'l') {
			/* DEC Private Mode Reset. */
			/* DEC Private Mode Set. */
			switch(m_esc_params[0]) {
			case 47:
			case 1047:
				/* Use Normal Screen Buffer. */
				m_active_buffer = 0;
				m_window->TerminalBufferChanged();
				break;
			case 1048:
				/* Restore Cursor. */
				m_buffers[m_active_buffer]->MoveCursor(m_saved_x, m_saved_y);
				break;
			case 1049:
				/* Restore Cursor and Use Normal Screen Buffer. */
				m_buffers[m_active_buffer]->MoveCursor(m_saved_x, m_saved_y);
				m_active_buffer = 0;
				m_window->TerminalBufferChanged();
				break;
			}
		}
		break;
	case 5:
		/* Don't support anything here yet. ESC( and ESC) only have one
		 * following character, if we're here we've received it so just
		 * ignore it and reset. */
		break;
	case 6:
		/* Operating System Controls. These take two arguments, an
		 * integer and a string. */
		if(m_esc_param_size < 0) {
			m_esc_string.clear();
			m_esc_param_size = 0;
		}

		if(m_esc_param_size == 0) {
			if(isdigit(raw)) {
				m_esc_params[0] *= 10;
				m_esc_params[0] += (raw - '0');
				return;
			} else if(raw == ';') {
				m_esc_param_size++;
				return;
			}
		} else if(raw == 7) {
			/* BEL is end of the command. */
			switch(m_esc_params[0]) {
			case 0:
			case 2:
				/* Set Window Title. */
				m_window->SetTitle(m_esc_string);
				break;
			};
		} else {
			if(isprint(raw)) {
				m_esc_string += raw;
				return;
			}
		}
		break;
	}

	/* If we get here then we either completed a sequence or found an
	 * invalid code. Both of these cases mean we should reset the state
	 * to 0. */
	m_esc_state = 0;
}

/** Get the active buffer.
 * @return		Active buffer. */
TerminalBuffer *Xterm::GetBuffer() {
	return m_buffers[m_active_buffer];
}
