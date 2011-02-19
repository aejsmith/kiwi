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
 * @brief		Terminal class.
 */

#ifndef __TERMINAL_H
#define __TERMINAL_H

#include <kiwi/Graphics/Rect.h>

#include <kiwi/Handle.h>
#include <kiwi/Process.h>

#include "TerminalBuffer.h"

/** Class implementing a terminal. */
class Terminal : public kiwi::Handle {
public:
	/** Base class for a terminal handler. */
	class Handler {
	public:
		virtual void Resize(int cols, int rows) = 0;
		virtual void Output(unsigned char raw) = 0;
		virtual TerminalBuffer *GetBuffer() = 0;

		/** Signal emitted when the active buffer changes. */
		kiwi::Signal<> OnBufferChange;
	};

	Terminal(Handler *handler, int cols, int rows);
	~Terminal();

	bool Run(const char *cmdline);
	void Resize(int cols, int rows);
	void Input(unsigned char ch);
	void Output(unsigned char ch);

	/** Get the ID of the terminal.
	 * @return		ID of the terminal. */
	int GetID() const { return m_id; }

	/** Get the size of the terminal.
	 * @param cols		Where to store number of columns.
	 * @param rows		Where to store number of rows. */
	void GetSize(int &cols, int &rows) const {
		cols = m_cols;
		rows = m_rows;
	}

	/** Get the data buffer.
	 * @return		Data buffer. */
	TerminalBuffer *GetBuffer() { return m_handler->GetBuffer(); }

	/** Signal emitted when the terminal main process exits.
	 * @param		Exit status code. */
	kiwi::Signal<int> OnExit;
private:
	void RegisterEvents();
	void HandleEvent(int event);

	int m_id;			/**< ID of the terminal. */
	Handler *m_handler;		/**< Handler for terminal input. */
	int m_cols;			/**< Width of the terminal. */
	int m_rows;			/**< Height of the terminal. */
	kiwi::Process m_process;	/**< Process running in the terminal. */
};

#endif /* __TERMINAL_H */
