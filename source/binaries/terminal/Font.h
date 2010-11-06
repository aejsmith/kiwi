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
 * @brief		Font class.
 */

#ifndef __FONT_H
#define __FONT_H

#include <kiwi/Graphics/Point.h>
#include <kiwi/Graphics/Surface.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/** Class representing a font. */
class Font : kiwi::Noncopyable {
public:
	Font(const char *path, double size);

	void DrawChar(cairo_t *context, unsigned char ch, kiwi::Point pos);
	kiwi::Size GetSize();
private:
	FT_Face m_ft_face;			/**< FreeType font face. */
	cairo_font_face_t *m_cairo_face;	/**< Cairo font face. */
	cairo_scaled_font_t *m_font;		/**< Actual font that is used. */

	static FT_Library m_library;
	static bool m_library_inited;
};

#endif /* __FONT_H */
