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
 */

#ifndef __XTERM_H
#define __XTERM_H

#include <string>

#include "Terminal.h"

class TerminalWindow;

/** Class implementing an Xterm emulator. */
class Xterm : public Terminal::Handler {
public:
	Xterm(TerminalWindow *window);
	~Xterm();

	void Resize(int cols, int rows);
	void Output(unsigned char raw);
	TerminalBuffer *GetBuffer();
private:
	TerminalWindow *m_window;	/**< Window that the terminal is on. */
	TerminalBuffer *m_buffers[2];	/**< Main and alternate buffer. */
	int m_active_buffer;		/**< Index of active buffer. */

	int m_esc_state;		/**< Current escape sequence parse state. */
	int m_esc_params[8];		/**< Escape code parameters. */
	int m_esc_param_size;		/**< Number of escape code parameters. */
	std::string m_esc_string;	/**< String escape code parameter. */

	/** Current attributes. */
	TerminalBuffer::Character m_attrib;

	int m_saved_x;			/**< Saved cursor X position. */
	int m_saved_y;			/**< Saved cursor Y position. */
};

#endif /* __XTERM_H */
