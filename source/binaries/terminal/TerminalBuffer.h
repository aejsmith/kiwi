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

#ifndef __TERMINALBUFFER_H
#define __TERMINALBUFFER_H

#include <kiwi/Graphics/Rect.h>

#include <deque>
#include <stdint.h>
#include <vector>

class TerminalWindow;

/** Class implementing a terminal data buffer. */
class TerminalBuffer {
	/** Maximum history size. */
	static const size_t kHistorySize = 2048;
public:
	/** Colour of a character in the buffer. */
	enum Colour : uint8_t {
		kBlackColour = 0,
		kRedColour = 1,
		kGreenColour = 2,
		kYellowColour = 3,
		kBlueColour = 4,
		kMagentaColour = 5,
		kCyanColour = 6,
		kWhiteColour = 7,

		/** No specific colour, use default. */
		kDefaultColour,
	};

	/** Character in a terminal buffer. */
	struct Character {
		unsigned char ch;	/**< Actual character. */
		Colour fg;		/**< Foreground colour. */
		Colour bg;		/**< Background colour. */
		bool bold;		/**< Bold. */
	};
private:
	/** Class containing a line in the buffer. */
	class Line {
	public:
		Line() {}

		/** Get a character from the line.
		 * @param idx		Index of the character. If the index is
		 *			out of bounds, a blank character will be
		 *			returned. */
		Character GetCharacter(size_t idx) const {
			if(idx < m_chars.size()) {
				return m_chars[idx];
			} else {
				Character ch = { ' ', kDefaultColour, kDefaultColour, false };
				return ch;
			}
		}

		/** Add a character to the line.
		 * @param idx		Index to add at.
		 * @param ch		Character to add. */
		void AddCharacter(size_t idx, Character ch) {
			if(idx >= m_chars.size()) {
				Character ch = { ' ', kDefaultColour, kDefaultColour, false };
				m_chars.resize(idx + 1, ch);
			}

			m_chars[idx] = ch;
		}

		/** Clear the contents of the line. */
		void Clear() {
			m_chars.clear();
		}
	private:
		/** Characters in the line. */
		std::vector<Character> m_chars;
	};
public:
	TerminalBuffer(TerminalWindow *window, int cols, int rows, bool history);
	~TerminalBuffer();

	void Resize(int cols, int rows);
	void ClearLine(int start_x, int end_x);
	void ClearLines(int start_y, int end_y);
	void ScrollUp();
	void ScrollDown();
	void SetScrollRegion(int top, int bottom);
	void MoveCursor(int x, int y);
	void Output(Character ch);

	Character CharAt(int x, int y);

	/** Get the current size of the history.
	 * @return		Current size of the history. */
	size_t GetHistorySize() const { return m_history.size(); }

	/** Get the size of the terminal.
	 * @param cols		Where to store number of columns.
	 * @param rows		Where to store number of rows. */
	void GetSize(int &cols, int &rows) const {
		cols = m_cols;
		rows = m_rows;
	}

	/** Get the current position of the cursor.
	 * @param x		Where to store X position.
	 * @param y		Where to store Y position. */
	void GetCursor(int &x, int &y) const {
		x = m_cursor_x;
		y = m_cursor_y;
	}
private:
	std::deque<Line *> m_lines;	/**< Lines in the main area. */
	std::deque<Line *> m_history;	/**< Lines in the history. */
	TerminalWindow *m_window;	/**< Window displaying the buffer. */
	int m_cols;			/**< Number of columns in the main area. */
	int m_rows;			/**< Number of rows in the main area. */
	bool m_use_history;		/**< Whether keeping history. */
	int m_cursor_x;			/**< X position of cursor. */
	int m_cursor_y;			/**< Y position of cursor. */
	int m_scroll_top;		/**< Top of scroll region. */
	int m_scroll_bottom;		/**< Bottom of scroll region. */
};

#endif /* __TERMINALBUFFER_H */
