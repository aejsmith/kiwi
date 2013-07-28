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
