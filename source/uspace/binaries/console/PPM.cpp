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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "PPM.h"

/* Skip over whitespace and comments in a PPM file.
 * @param buf		Pointer to data buffer.
 * @return		Address of next non-whitespace byte. */
static unsigned char *ppm_skip(unsigned char *buf) {
	while(true) {
		while(isspace(*buf)) {
			buf++;
		}

		if(*buf == '#') {
			while(*buf != '\n' && *buf != '\r') {
				buf++;
			}
		} else {
			break;
		}
	}

	return buf;
}

/** Constructs the object and reads in the image.
 * @param buf		Pointer to memory buffer containing image.
 * @param size		Size of memory buffer. */
PPM::PPM(unsigned char *buf, size_t size) : m_buffer(0), m_width(0), m_height(0) {
	unsigned int max_colour, coef;
	size_t i;

	if((buf[0] != 'P') || (buf[1] != '6')) {
		return;
	}
	buf += 2;

	buf = ppm_skip(buf);
	m_width = strtoul(reinterpret_cast<const char *>(buf), reinterpret_cast<char **>(&buf), 10);
	buf = ppm_skip(buf);
	m_height = strtoul(reinterpret_cast<const char *>(buf), reinterpret_cast<char **>(&buf), 10);
	buf = ppm_skip(buf);
	max_colour = strtoul(reinterpret_cast<const char *>(buf), reinterpret_cast<char **>(&buf), 10);
	buf++;

	if(!max_colour || max_colour > 255) {
		m_width = m_height = 0;
		return;
	}

	/* Allocate the data buffer. */
	m_buffer = new RGB[m_width * m_height];

	coef = 255 / max_colour;
	if((coef * max_colour) > 255) {
		coef -= 1;
	}

	/* Read the data into the buffer. */
	for(i = 0; i < (m_width * m_height); i++) {
		m_buffer[i].r = *(buf++) * coef;
		m_buffer[i].g = *(buf++) * coef;
		m_buffer[i].b = *(buf++) * coef;
	}
}

/** Destroy the object. */
PPM::~PPM() {
	delete[] m_buffer;
}

/** Write the image to a framebuffer.
 * @param fb		Framebuffer to write to.
 * @param x		X position to write at.
 * @param y		Y position to write it. */
void PPM::Draw(Framebuffer *fb, int x, int y) {
	fb->DrawRect(x, y, m_width, m_height, m_buffer);
}
