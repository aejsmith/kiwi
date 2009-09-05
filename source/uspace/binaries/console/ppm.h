/* Kiwi console PPM reader class
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
 * @brief		PPM reader class.
 */

#ifndef __PPM_H
#define __PPM_H

#include "fb.h"

/** Class implementing a PPM image reader. */
class PPM {
public:
	/** Constructs the object and reads in the image.
	 * @param buf		Pointer to memory buffer containing image.
	 * @param size		Size of memory buffer. */
	PPM(unsigned char *buf, size_t size);

	/** Destroy the object. */
	~PPM();

	/** Write the image to a framebuffer.
	 * @param fb		Framebuffer to write to.
	 * @param x		X position to write at.
	 * @param y		Y position to write it. */
	void Draw(Framebuffer *fb, int x, int y);

	/** Get the width of the image.
	 * @return		Width of image. */
	size_t Width(void) { return m_width; }

	/** Get the height of the image.
	 * @return		Height of image. */
	size_t Height(void) { return m_height; }
private:
	RGB *m_buffer;			/**< Buffer containing image. */
	size_t m_width;			/**< Width of image. */
	size_t m_height;		/**< Height of image. */
};

extern unsigned char logo_ppm[];
extern unsigned int logo_ppm_size;

#endif /* __PPM_H */
