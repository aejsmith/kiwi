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
 * @brief		Window decoration class.
 */

#include <cairo/cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <exception>
#include <iostream>
#include <math.h>

#include "Decoration.h"
#include "ServerWindow.h"
#include "Session.h"

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
Decoration::Decoration(ServerWindow *window) :
	m_window(window), m_surface(0), m_grabbed(false)
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

	Update();
}

/** Destroy the decoration. */
Decoration::~Decoration() {
	if(m_surface) {
		cairo_surface_destroy(m_surface);
	}
}

/** Regenerate the decoration. */
void Decoration::Update() {
	cairo_pattern_t *pat;
	cairo_t *context;

	if(m_surface) {
		cairo_surface_destroy(m_surface);
	}

	/* Work out the new size. */
	m_frame = Rect(-1, -24, m_window->GetFrame().GetWidth() + 2, m_window->GetFrame().GetHeight() + 25);

	/* Create a new surface and a context to render to. */
	m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, m_frame.GetWidth(), m_frame.GetHeight());
	context = cairo_create(m_surface);

	/* Draw the frame. */
	rounded_rectangle(context, 0, 0, m_frame.GetWidth(), m_frame.GetHeight(), 7.5, CORNER_TOP_LEFT | CORNER_TOP_RIGHT);
	cairo_set_source_rgb(context, 0.253906, 0.253906, 0.253906);
	cairo_fill(context);
	rounded_rectangle(context, 1, 1, m_frame.GetWidth() - 2, 11, 7.5, CORNER_TOP_LEFT | CORNER_TOP_RIGHT);
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
	cairo_rectangle(context, 1, 12, m_frame.GetWidth() - 2, 12);
	pat = cairo_pattern_create_linear(0, 12, 0, 24);
	cairo_pattern_add_color_stop_rgb(pat, 0, 0.156250, 0.156250, 0.156250);
	cairo_pattern_add_color_stop_rgb(pat, 1, 0.089844, 0.089844, 0.089844);
	cairo_set_source(context, pat);
	cairo_fill(context);
	cairo_pattern_destroy(pat);
	if(m_window->IsActive()) {
		cairo_rectangle(context, 1, 23, m_frame.GetWidth() - 2, 1);
		pat = cairo_pattern_create_linear(0, 23, m_frame.GetWidth(), 23);
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
	cairo_rectangle(context, 1, 24, m_window->GetFrame().GetWidth(), m_window->GetFrame().GetHeight());
	cairo_set_source_rgba(context, 0, 0, 0, 0);
	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	cairo_fill(context);

	cairo_destroy(context);
}

/** Handle a mouse move event on the decoration.
 * @param event		Event information object. */
void Decoration::MouseMoved(const MouseEvent &event) {
	if(m_grabbed && m_window->GetStyle() & BaseWindow::kMovableMask) {
		Point delta = event.GetPosition() - m_grab_pos;
		m_window->MoveTo(m_window->GetFrame().GetTopLeft() + delta);
	}
}

/** Handle a mouse press event on the decoration.
 * @param event		Event information object. */
void Decoration::MousePressed(const MouseEvent &event) {
	if(event.GetButtons() & Input::kLeftButton) {
		m_grabbed = true;
		m_grab_pos = event.GetPosition();

		/* Grab the mouse in the session. */
		m_window->GetSession()->GrabMouse(this);
	}
}

/** Handle a mouse release event on the decoration.
 * @param event		Event information object. */
void Decoration::MouseReleased(const MouseEvent &event) {
	if(!(event.GetButtons() & Input::kLeftButton) && m_grabbed) {
		m_grabbed = false;
		m_window->GetSession()->ReleaseMouse();
	}
}

/** Get a point relative to the decoration.
 * @return		Point relative to the decoration. */
Point Decoration::RelativePoint(kiwi::Point pos) const {
	return m_window->RelativePoint(pos);
}
