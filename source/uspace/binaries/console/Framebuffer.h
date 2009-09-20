/* Kiwi console framebuffer class
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
 * @brief		Framebuffer class.
 */

#ifndef __FRAMEBUFFER_H
#define __FRAMEBUFFER_H

#include <drivers/display.h>

/** RGB colour structure. */
struct RGB {
	uint8_t r, g, b;
} __attribute__((packed));

/** Framebuffer class. */
class Framebuffer {
public:
	Framebuffer(const char *device);
	~Framebuffer();

	/** Check if initialisation succeeded.
	 * @return		Whether initialisation succeeded. */
	bool Initialised(void) const { return (m_init_status == 0); }

	/** Get the framebuffer width.
	 * @return		Framebuffer width. */
	size_t Width(void) { return m_width; }

	/** Get the framebuffer height.
	 * @return		Framebuffer height. */
	size_t Height(void) { return m_height; }

	RGB GetPixel(int x, int y);
	void PutPixel(int x, int y, RGB colour);
	void FillRect(int x, int y, int width, int height, RGB colour);
	void DrawRect(int x, int y, int width, int height, RGB *buffer);
private:
	static void _Callback(void *arg);

	int m_init_status;		/**< Initialisation status. */
	uint8_t *m_buffer;		/**< Mapping of display device memory. */
	size_t m_buffer_size;		/**< Size of mapping. */
	handle_t m_handle;		/**< Handle to device. */
	size_t m_width;			/**< Display width. */
	size_t m_height;		/**< Display height. */
	size_t m_depth;			/**< Display depth. */
};

#endif /* __FRAMEBUFFER_H */
