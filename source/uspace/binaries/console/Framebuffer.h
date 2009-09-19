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
	/** Constructor for a Framebuffer object.
	 * @param handle	Handle for display device. The object takes
	 *			ownership of this - do not close it.
	 * @param mode		Display mode for device. Values are copied out
	 *			of the structure - can be freed after. */
	Framebuffer(handle_t handle, display_mode_t *mode);
	~Framebuffer();

	/** Check if initialisation succeeded.
	 * @return		0 if succeeded, negative error code if not. */
	int InitCheck(void) const { return m_init_status; }

	/** Get a pixel from the screen.
	 * @param x		X position.
	 * @param y		Y position.
	 * @return		Pixel colour. */
	RGB GetPixel(int x, int y);

	/** Put a pixel on the screen.
	 * @param x		X position.
	 * @param y		Y position.
	 * @param colour	Colour to write in. */
	void PutPixel(int x, int y, RGB colour);

	/** Fill an area with a solid colour.
	 * @param x		X position to start at.
	 * @param y		Y position to start at.
	 * @param width		Width of rectangle.
	 * @param height	Height of rectangle.
	 * @param colour	Colour to use. */
	void FillRect(int x, int y, int width, int height, RGB colour);

	/** Write a rectangle to the screen.
	 * @param x		X position to start at.
	 * @param y		Y position to start at.
	 * @param width		Width of rectangle.
	 * @param height	Height of rectangle.
	 * @param buffer	Buffer containing rectangle to write. */
	void DrawRect(int x, int y, int width, int height, RGB *buffer);
private:
	int m_init_status;		/**< Initialisation status. */
	uint8_t *m_buffer;		/**< Mapping of display device memory. */
	size_t m_buffer_size;		/**< Size of mapping. */
	handle_t m_handle;		/**< Handle to device. */
	size_t m_width;			/**< Display width. */
	size_t m_height;		/**< Display height. */
	size_t m_depth;			/**< Display depth. */
};

#endif /* __FRAMEBUFFER_H */
