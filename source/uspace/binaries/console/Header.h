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

#ifndef __HEADER_H
#define __HEADER_H

#include "Framebuffer.h"
#include "PPM.h"

/** Class containing a console header. */
class Header {
public:
	void Draw(Framebuffer *fb);

	/** Get the header height.
	 * @return		Height of header. */
	int Height(void) { return m_logo.Height() + 1; }

	/** Retreive the singleton instance.
	 * @return		Pointer to instance. */
	static Header *Instance(void) { return &m_instance; }
private:
	Header();

	static Header m_instance;	/**< Singleton instance. */

	PPM m_logo;			/**< Logo image. */
};

#endif /* __HEADER_H */
