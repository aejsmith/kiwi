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
 * @brief		Window decoration class.
 */

#include <cairo/cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <exception>
#include <iostream>

#include "Decoration.h"
#include "Window.h"

using namespace kiwi;
using namespace std;

/** FreeType library instance. */
static FT_Library g_freetype;

/** Cairo font face/options. */
static cairo_font_face_t *g_cairo_font = 0;
static cairo_font_options_t *g_cairo_font_opts = 0;

/** Corners to include in a rounded rectangle. */
enum {
	CORNER_TOP_LEFT = (1<<0),
	CORNER_TOP_RIGHT = (1<<1),
	CORNER_BOTTOM_LEFT = (1<<2),
	CORNER_BOTTOM_RIGHT = (1<<3),
};

/** Set the current path to a rounded rectangle.
 * @param context	Context to draw on.
 * @param x		X position.
 * @param y		Y position.
 * @param width		Width.
 * @param height	Height.
 * @param radius	Corner radius. */
static void rounded_rectangle(cairo_t *context, double x, double y, double width,
                              double height, double radius, int corners) {
	double degrees = M_PI / 180.0;

	cairo_new_sub_path(context);
	if(corners & CORNER_TOP_RIGHT) {
		cairo_arc(context, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
	} else {
		cairo_line_to(context, x + width, y);
	}
	if(corners & CORNER_BOTTOM_RIGHT) {
		cairo_arc(context, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
	} else {
		cairo_line_to(context, x + width, y + height);
	}
	if(corners & CORNER_BOTTOM_LEFT) {
		cairo_arc(context, x + width - radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
	} else {
		cairo_line_to(context, x, y + height);
	}
	if(corners & CORNER_TOP_LEFT) {
		cairo_arc(context, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
	} else {
		cairo_line_to(context, x, y);
	}
	cairo_close_path(context);
}

/** Create a decoration.
 * @param window	Window that the decoration is for. */
Decoration::Decoration(Window *window) :
	m_window(window), m_surface(0)
{
	if(!g_cairo_font) {
		FT_Face face;
		int error;

		/* Initialise FreeType. */
		error = FT_Init_FreeType(&g_freetype);
		if(error) {
			clog << "Failed to initialise FreeType: " << error << endl;
			throw exception();
		}

		/* Load the font. */
		error = FT_New_Face(g_freetype, "/system/data/fonts/DejaVuSans-Bold.ttf", 0, &face);
		if(error) {
			clog << "Failed to load font: " << error << endl;
			throw exception();
		}

		/* Create the Cairo font face. */
		g_cairo_font = cairo_ft_font_face_create_for_ft_face(face, 0);
		g_cairo_font_opts = cairo_font_options_create();
		cairo_font_options_set_antialias(g_cairo_font_opts, CAIRO_ANTIALIAS_SUBPIXEL);
		cairo_font_options_set_hint_style(g_cairo_font_opts, CAIRO_HINT_STYLE_FULL);
		cairo_font_options_set_hint_metrics(g_cairo_font_opts, CAIRO_HINT_METRICS_ON);
	}

	Redraw();
}

/** Destroy the decoration. */
Decoration::~Decoration() {
	if(m_surface) {
		cairo_surface_destroy(m_surface);
	}
}

/** Regenerate the decoration. */
void Decoration::Redraw() {
	cairo_pattern_t *pat;
	cairo_t *context;

	if(m_surface) {
		cairo_surface_destroy(m_surface);
	}

	/* Work out the new size. */
	m_rect = Rect(-1, -24, m_window->GetRect().GetWidth() + 2, m_window->GetRect().GetHeight() + 25);

	/* Create a new surface and a context to render to. */
	m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_rect.GetWidth(), m_rect.GetHeight());
	context = cairo_create(m_surface);

	/* Draw the frame. */
	rounded_rectangle(context, 0, 0, m_rect.GetWidth(), m_rect.GetHeight(), 7.5, CORNER_TOP_LEFT | CORNER_TOP_RIGHT);
	cairo_set_source_rgb(context, 0.253906, 0.253906, 0.253906);
	cairo_fill(context);
	rounded_rectangle(context, 1, 1, m_rect.GetWidth() - 2, 11, 7.5, CORNER_TOP_LEFT | CORNER_TOP_RIGHT);
	pat = cairo_pattern_create_linear(0, 0, 0, 12);
	if(m_window->IsActive()) {
		cairo_pattern_add_color_stop_rgb(pat, 0, 0.601562, 0.601562, 0.601562);
		cairo_pattern_add_color_stop_rgb(pat, 1, 0.328125, 0.328125, 0.328125);
	} else {
		cairo_pattern_add_color_stop_rgb(pat, 0, 0.314531, 0.314531, 0.314531);
		cairo_pattern_add_color_stop_rgb(pat, 1, 0.194375, 0.194375, 0.194375);
	}
	cairo_set_source(context, pat);
	cairo_fill(context);
	cairo_pattern_destroy(pat);
	cairo_rectangle(context, 1, 12, m_rect.GetWidth() - 2, 12);
	pat = cairo_pattern_create_linear(0, 12, 0, 24);
	cairo_pattern_add_color_stop_rgb(pat, 0, 0.156250, 0.156250, 0.156250);
	cairo_pattern_add_color_stop_rgb(pat, 1, 0.089844, 0.089844, 0.089844);
	cairo_set_source(context, pat);
	cairo_fill(context);
	cairo_pattern_destroy(pat);
	if(m_window->IsActive()) {
		cairo_rectangle(context, 1, 23, m_rect.GetWidth() - 2, 1);
		pat = cairo_pattern_create_linear(0, 23, m_rect.GetWidth(), 23);
		cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 0);
		cairo_pattern_add_color_stop_rgb(pat, 0.5, 0.45, 0.45, 0.45);
		cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 0);
		cairo_set_source(context, pat);
		cairo_fill(context);
		cairo_pattern_destroy(pat);
	}

	/* Write the title. */
	if(m_window->IsActive()) {
		cairo_set_source_rgb(context, 1, 1, 1);
	} else {
		cairo_set_source_rgb(context, 0.45, 0.45, 0.45);
	}
	cairo_set_font_face(context, g_cairo_font);
	cairo_set_font_options(context, g_cairo_font_opts);
	cairo_set_font_size(context, 12.5);
	cairo_move_to(context, 7, 17);
	cairo_show_text(context, m_window->GetTitle().c_str());

	/* Clear the window content area. */
	cairo_rectangle(context, 1, 24, m_window->GetRect().GetWidth(), m_window->GetRect().GetHeight());
	cairo_set_source_rgba(context, 0, 0, 0, 0);
	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	cairo_fill(context);

	cairo_destroy(context);
}
