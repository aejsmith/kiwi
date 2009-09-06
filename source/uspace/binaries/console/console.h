/* Kiwi console definitions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Console definitions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include "fb.h"

/** Main console implementation. */
class Console {
public:
	/** Constructor for the console.
	 * @param fb		Framebuffer to use.
	 * @param x		X position.
	 * @param y		Y position.
	 * @param width		Width.
	 * @param height	Height. */
	Console(Framebuffer *fb, int x, int y, int width, int height);
	~Console();

	/** Check if initialisation succeeded.
	 * @return		0 if succeeded, negative error code if not. */
	int InitCheck(void) const { return m_init_status; }

	/** Run a command within a console.
	 * @param cmdline	Command line to run.
	 * @return		0 if command started successfully, negative
	 *			error code on failure. */
	int Run(const char *cmdline);

	/** Add input to the console.
	 * @param ch		Input character. */
	void Input(unsigned char ch);

	/** Output a character to the console.
	 * @param ch		Output character. */
	void Output(unsigned char ch);

	/** Redraw the console. */
	void Redraw(void);

	/** Get the active console.
	 * @return		Pointer to active console. */
	static Console *GetActive(void) { return m_active; }
private:
	/** Invert the cursor state at the current position. */
	void ToggleCursor(void);

	/** Put a character on the console.
	 * @param ch		Character to place. */
	void PutChar(unsigned char ch);

	/** Clear the console. */
	void Clear(void);

	/** Scroll up one line. */
	void ScrollUp(void);

	/** Scroll down one line. */
	void ScrollDown(void);

	/** Thread function.
	 * @param arg		Thread argument (console object pointer). */
	static void _ThreadEntry(void *arg);

	static Console *m_active;	/**< Active console. */

	int m_init_status;		/**< Initialisation status. */
	handle_t m_thread;		/**< Thread that processes output. */
	handle_t m_master;		/**< Handle to console master device. */
	identifier_t m_id;		/**< Console ID. */

	Framebuffer *m_fb;		/**< Framebuffer. */
	RGB *m_buffer;			/**< Back buffer. */
	int m_fb_x;			/**< X position of console on framebuffer. */
	int m_fb_y;			/**< Y position of console on framebuffer. */
	int m_width_px;			/**< Console width (in pixels). */
	int m_height_px;		/**< Console height (in pixels). */

	int m_cursor_x;			/**< Cursor X position (in characters). */
	int m_cursor_y;			/**< Cursor Y position (in characters). */
	int m_cols;			/**< Number of columns. */
	int m_rows;			/**< Number of rows. */
	int m_scroll_start;		/**< First line of scroll region. */
	int m_scroll_end;		/**< Last line of scroll region. */

	RGB m_fg_colour;		/**< Foreground colour. */
	RGB m_bg_colour;		/**< Background colour. */
};

#endif /* __CONSOLE_H */
