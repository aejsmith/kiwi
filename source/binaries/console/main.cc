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
 * @brief		Console application.
 */

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Graphics/Surface.h>
#include <kiwi/EventLoop.h>

#include <algorithm>
#include <iostream>
#include <cstring>
#include <math.h>

using namespace kiwi;
using namespace std;

/* 600x450. */
static void draw_window(cairo_t *context) {
	cairo_font_face_t *cfont;
	FT_Library freetype;
	FT_Face face;

	FT_Init_FreeType(&freetype);
	FT_New_Face(freetype, "/system/data/fonts/DejaVuSansMono.ttf", 0, &face);
	cfont = cairo_ft_font_face_create_for_ft_face(face, 0);

	cairo_save(context);

	cairo_rectangle(context, 0, 0, 650, 500);
	cairo_set_source_rgba(context, 0, 0, 0, 0.9);
	cairo_fill(context);

	cairo_set_source_rgb(context, 1, 1, 1);
	cairo_set_font_face(context, cfont);
	cairo_set_font_size(context, 12.0);
	cairo_move_to(context, 3, 13);
	cairo_show_text(context, "Hello, World!");

	cairo_restore(context);
}

int main(int argc, char **argv) {
	EventLoop loop;

	BaseWindow window(Rect(150, 150, 650, 500));
	window.SetTitle("Console");

	cairo_surface_t *surface = window.GetSurface()->GetCairoSurface();
	cairo_t *context = cairo_create(surface);
	draw_window(context);
	window.Show();

	loop.Run();
	return 0;
}
