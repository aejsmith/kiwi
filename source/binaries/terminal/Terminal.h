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
 * @brief		Terminal class.
 */

#ifndef __TERMINAL_H
#define __TERMINAL_H

#include <kiwi/Graphics/Rect.h>

#include <kiwi/Error.h>
#include <kiwi/Handle.h>
#include <kiwi/Process.h>

/** Class implementing a terminal. */
class Terminal : public kiwi::Handle {
public:
#if 0
	/** Buffer containing the data from the terminal. */
	class Buffer : kiwi::Noncopyable {
		/** Maximum number of rows in the buffer. */
		static const int kMaximumRows = 1024;
	public:
		Buffer(int cols);
		~Buffer();

		void Resize(int cols);
		void ChangeOffset(int delta);
		void WriteChar(

		/** Signal emitted when a visible area must be update.
		 * @param		Area to update. Note that these
		 *			coordinates are in cells, not pixels. */
		kiwi::Signal<Rect> OnUpdate;
	private:
		size_t m_start;		/**< Start of the circular buffer. */
		size_t m_end;		/**< End of the circular buffer. */
		size_t m_offset;	/**< Current display offset. */

		/** Array of rows in the buffer. */
		char *m_data[kMaximumRows];
	};
#endif
	Terminal(int cols, int rows);
	~Terminal();

	bool Run(const char *cmdline);
	void Resize(int cols, int rows);
	void Input(unsigned char ch);
	void Output(unsigned char ch);

	/** Get the size of the terminal.
	 * @param cols		Where to store number of columns.
	 * @param rows		Where to store number of rows. */
	void GetSize(int &cols, int &rows) const {
		cols = m_columns;
		rows = m_rows;
	}

	/** Get the ID of the terminal.
	 * @return		ID of the terminal. */
	int GetID() const { return m_id; }

	/** Get the data buffer.
	 * @return		Data buffer. */
	char **GetBuffer() { return m_buffer; }

	/** Signal emitted when the terminal main process exits.
	 * @param		Exit status code. */
	kiwi::Signal<int> OnExit;

	/** Signal emitted when an area of the terminal is updated.
	 * @param		Rectangle area to update. */
	kiwi::Signal<kiwi::Rect> OnUpdate;

	/** Signal emitted when the terminal is scrolled down. */
	kiwi::Signal<> OnScrollDown;
private:
	void ScrollDown();

	void RegisterEvents();
	void HandleEvent(int event);

	int m_id;			/**< ID of the terminal. */
	int m_columns;			/**< Width of the terminal. */
	int m_rows;			/**< Height of the terminal. */
	int m_cursor_x;			/**< X position of cursor. */
	int m_cursor_y;			/**< Y position of cursor. */
	kiwi::Process m_process;	/**< Process running in the terminal. */
	char **m_buffer;		/**< Data buffer. */
};

#endif /* __TERMINAL_H */
