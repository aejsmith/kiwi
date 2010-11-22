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
