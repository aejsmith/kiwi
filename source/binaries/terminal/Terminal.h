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
