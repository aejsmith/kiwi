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

#include <cairo/cairo-ft.h>
#include <math.h>
#include "Font.h"

using namespace kiwi;

/** FreeType library instance. */
FT_Library Font::m_library;
bool Font::m_library_inited = false;

/** Initialise the font.
 * @param path		Path to font file.
 * @param size		Size of font. */
Font::Font(const char *path, double size) {
	cairo_font_options_t *options;
	cairo_matrix_t matrix, ctm;

	/* Initialise the library if necessary. */
	if(!m_library_inited) {
		FT_Init_FreeType(&m_library);
		m_library_inited = true;
	}

	/* Open the font. */
	FT_New_Face(m_library, path, 0, &m_ft_face);
	m_cairo_face = cairo_ft_font_face_create_for_ft_face(m_ft_face, 0);

	/* Set up font options. */
	options = cairo_font_options_create();
	cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_FULL);
	cairo_matrix_init_scale(&matrix, size, size);
	cairo_matrix_init_identity(&ctm);

	/* Create the real Cairo font. */
	m_font = cairo_scaled_font_create(m_cairo_face, &matrix, &ctm, options);
	cairo_font_options_destroy(options);
}

/** Draw a character in a Cairo context.
 * @param context	Context to draw to. Source should be set.
 * @param ch		Character to draw.
 * @param pos		Position to draw at. */ 
void Font::DrawChar(cairo_t *context, unsigned char ch, Point pos) {
	cairo_font_extents_t extents;
	char buf[2] = { ch, 0 };

	cairo_scaled_font_extents(m_font, &extents);

	cairo_save(context);
	cairo_set_scaled_font(context, m_font);
	cairo_move_to(context, pos.GetX(), static_cast<double>(pos.GetY()) + extents.ascent);
	cairo_show_text(context, buf);
	cairo_restore(context);
}

/** Get the maximum width and height of the font.
 * @return		Size of the font (in pixels). */
Size Font::GetSize() {
	cairo_font_extents_t extents;
	int width, height;

	cairo_scaled_font_extents(m_font, &extents);
	width = round(extents.max_x_advance);
	height = round(extents.ascent + extents.descent);
	return Size(width, height);
}