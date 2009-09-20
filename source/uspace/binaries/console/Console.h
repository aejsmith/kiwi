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

#include "Framebuffer.h"

/** Main console implementation. */
class Console {
public:
	Console(Framebuffer *fb, int x, int y, int width, int height);
	~Console();

	/** Check if initialisation succeeded.
	 * @return		Whether initialisation succeeded. */
	bool Initialised(void) const { return (m_init_status == 0); }

	int Run(const char *cmdline);
	void Input(unsigned char ch);
	void Output(unsigned char ch);
	void Redraw(void);

	/** Get the active console.
	 * @return		Pointer to active console. */
	static Console *GetActive(void) { return m_active; }
private:
	void ToggleCursor(void);
	void PutChar(unsigned char ch);
	void Clear(void);
	void ScrollUp(void);
	void ScrollDown(void);

	static void _Callback(void *arg);

	static Console *m_active;	/**< Active console. */

	int m_init_status;		/**< Initialisation status. */
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
