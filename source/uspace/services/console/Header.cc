/* Kiwi console header class
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
 * @brief		Console header class.
 */

#include "Header.h"

/** Singleton header instance. */
Header Header::m_instance;

/** Header constructor. */
Header::Header() :
	m_logo(logo_ppm, logo_ppm_size)
{
}

/** Draw the header to a framebuffer.
 * @param fb		Framebuffer to draw to. */
void Header::Draw(Framebuffer *fb) {
	RGB grey = { 0x55, 0x55, 0x55 }, black = { 0, 0, 0 };

	/* Blank the space we're going to occupy. */
	fb->FillRect(0, 0, fb->Width(), m_logo.Height(), black);

	/* Write the logo to the framebuffer. */
	m_logo.Draw(fb, 0, 0);

	/* Draw a line under the logo. */
	fb->FillRect(0, m_logo.Height(), fb->Width(), 1, grey);
}
