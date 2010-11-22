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
 * @brief		Terminal buffer class.
 */

#include <algorithm>
#include <assert.h>
#include <stdlib.h>

#include "TerminalBuffer.h"
#include "TerminalWindow.h"

using namespace kiwi;

/** Initialise the terminal buffer.
 * @param window	Window that the buffer will be displayed on.
 * @param cols		Initial width of the terminal.
 * @param rows		Initial height of the terminal.
 * @param history	Whether to use history. */
TerminalBuffer::TerminalBuffer(TerminalWindow *window, int cols, int rows, bool history) :
	m_window(window), m_cols(cols), m_rows(rows), m_use_history(history),
	m_cursor_x(0), m_cursor_y(0), m_scroll_top(0), m_scroll_bottom(rows - 1)
{
	for(int i = 0; i < rows; i++) {
		m_lines.push_back(new Line);
	}
}

/** Destroy the terminal buffer. */
TerminalBuffer::~TerminalBuffer() {
	/* Free all lines in the history and the main area. */
	std::for_each(m_history.begin(), m_history.end(), [](Line *line) {
		delete line;
	});
	std::for_each(m_lines.begin(), m_lines.end(), [](Line *line) {
		delete line;
	});
}

/** Resize the terminal buffer.
 * @param cols		New width.
 * @param rows		New height. */
void TerminalBuffer::Resize(int cols, int rows) {

}

/** Clear part of the current line.
 * @param start_x	Start X position.
 * @param end_x		End X position (inclusive). */
void TerminalBuffer::ClearLine(int start_x, int end_x) {
	/* Check for valid values, correct them if they are bad. */
	if(start_x < 0) { start_x = 0; }
	if(start_x >= m_cols) { start_x = m_cols - 1; }
	if(end_x < 0) { end_x = 0; }
	if(end_x >= m_cols) { end_x = m_cols - 1; }
	if(start_x > end_x) { return; }

	Character ch = { ' ', kDefaultColour, kDefaultColour, false };
	for(int i = start_x; i <= end_x; i++) {
		m_lines[m_cursor_y]->AddCharacter(i, ch);
	}

	m_window->TerminalUpdated(Rect(start_x, m_cursor_y, (end_x - start_x) + 1, 1));
}

/** Clear lines on the buffer.
 * @param start_y	Start Y position.
 * @param end_y		End Y position (inclusive). */
void TerminalBuffer::ClearLines(int start_y, int end_y) {
	/* Check for valid values, correct them if they are bad. */
	if(start_y < 0) { start_y = 0; }
	if(start_y >= m_rows) { start_y = m_rows - 1; }
	if(end_y < 0) { end_y = 0; }
	if(end_y >= m_rows) { end_y = m_rows - 1; }
	if(start_y > end_y) { return; }

	for(int i = start_y; i <= end_y; i++) {
		m_lines[i]->Clear();
	}

	m_window->TerminalUpdated(Rect(0, start_y, m_cols, (end_y - start_y) + 1));
}

/** Scroll the buffer up (move contents down). */
void TerminalBuffer::ScrollUp() {
	if(m_scroll_top == 0 && m_scroll_bottom == (m_rows - 1)) {
		delete m_lines[m_lines.size() - 1];
		m_lines.pop_back();
		m_lines.push_front(new Line);
	} else {
		/* Remove a line from the end of the scroll region, then add a
		 * new line at the top of it. */
		m_lines.erase(m_lines.begin() + (m_scroll_bottom + 1));
		m_lines.insert(m_lines.begin() + m_scroll_top, new Line);
	}

	m_window->TerminalScrolled(m_scroll_top, m_scroll_bottom, 1);
}

/** Scroll the buffer down (move contents up). */
void TerminalBuffer::ScrollDown() {
	if(m_scroll_top == 0 && m_scroll_bottom == (m_rows - 1)) {
		/* Scroll region is equal to entire visible area: we can push
		 * the top line back to history. */
		m_history.push_back(m_lines[0]);
		if(m_history.size() > kHistorySize) {
			delete m_history[0];
			m_history.pop_front();
		}
		m_lines.pop_front();
		m_lines.push_back(new Line);
		m_window->TerminalHistoryAdded();
	} else {
		/* Insert a new line at the end of the scroll region, then
		 * remove the line at the top of it. */
		m_lines.insert(m_lines.begin() + (m_scroll_bottom + 1), new Line);
		m_lines.erase(m_lines.begin() + m_scroll_top);
		m_window->TerminalScrolled(m_scroll_top, m_scroll_bottom, -1);
	}
}

/** Set the scroll region.
 * @param top		Top row of the scroll region.
 * @param bottom	Bottom row of the scroll region (inclusive). */
void TerminalBuffer::SetScrollRegion(int top, int bottom) {
	/* Check the values. */
	if(top >= bottom || top < 0 || top >= m_rows || bottom < 0 || bottom >= m_rows) {
		top = 0;
		bottom = m_rows - 1;
	}

	m_scroll_top = top;
	m_scroll_bottom = bottom;
}

/** Move the cursor.
 * @param x		New X position of cursor.
 * @param y		New Y position of cursor. */
void TerminalBuffer::MoveCursor(int x, int y) {
	/* Check for valid values, correct them if they are bad. */
	if(x < 0) { x = 0; }
	if(x >= m_cols) { x = m_cols - 1; }
	if(y < 0) { y = 0; }
	if(y >= m_rows) { y = m_rows - 1; }

	m_cursor_x = x;
	m_cursor_y = y;
}

/** Output a character at the current cursor position.
 * @param ch		Character to output. */
void TerminalBuffer::Output(Character ch) {
	switch(ch.ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(m_cursor_x) {
			m_cursor_x--;
		} else if(m_cursor_y) {
			m_cursor_x = m_cols - 1;
			m_cursor_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		m_cursor_x = 0;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		m_cursor_x = 0;
		m_cursor_y++;
		break;
	case '\t':
		m_cursor_x += 8 - (m_cursor_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch.ch < ' ') {
			break;
		}

		m_lines[m_cursor_y]->AddCharacter(m_cursor_x, ch);
		m_window->TerminalUpdated(Rect(m_cursor_x, m_cursor_y, 1, 1));
		m_cursor_x++;
		break;
	}

	/* If we have reached the edge of the console insert a new line. */
	if(m_cursor_x >= m_cols) {
		m_cursor_x = 0;
		m_cursor_y++;
	}

	/* If we have reached the bottom of the scroll region, scroll. */
	if(m_cursor_y > m_scroll_bottom) {
		ScrollDown();
		m_cursor_y = m_rows - 1;
	}
}

/** Get a character from the buffer.
 * @param x		X position to get from (if out of bounds, a blank
 *			character will be returned).
 * @param y		Y position to get from (if negative, will get from
 *			history).
 * @return		Character read. */
TerminalBuffer::Character TerminalBuffer::CharAt(int x, int y) {
	if(y < 0) {
		assert(abs(y) <= m_history.size());
		return (*(m_history.end() - abs(y)))->GetCharacter(x);
	} else {
		assert(y < m_rows);
		return m_lines[y]->GetCharacter(x);
	}
}
