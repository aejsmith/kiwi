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
 * @brief		UI test application.
 */

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <kernel/area.h>
#include <kernel/vm.h>
#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <kiwi/EventLoop.h>

#include <algorithm>
#include <iostream>
#include <math.h>

#include "org.kiwi.WindowServer.h"

using namespace org::kiwi::WindowServer;
using namespace std;

template <typename T>
static inline T p2align(T n, int alignment) {
	if(n % alignment) {
		n += alignment;
		n -= n % alignment;
	}
	return n;
}

ServerConnection *g_conn;

/* 300x300. */
static void draw_pattern1(cairo_t *context) {
	double x = 20, y = 20, width = 260, height = 260;
	double aspect = 1.0;
	double corner_radius = height / 10.0;
	double radius = corner_radius / aspect;
	double degrees = M_PI / 180.0;

	cairo_save(context);

	cairo_rectangle(context, 0, 0, 300, 300);
	cairo_set_source_rgb(context, 0, 0, 0);
	cairo_fill(context);

	cairo_new_sub_path(context);
	cairo_arc(context, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
	cairo_arc(context, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
	cairo_arc(context, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
	cairo_arc(context, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
	cairo_close_path(context);

	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(context, 0.5, 0.5, 1, 0.8);
	cairo_fill_preserve(context);
	cairo_set_operator(context, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(context, 0.5, 0, 0, 0.5);
	cairo_set_line_width(context, 10.0);
	cairo_stroke(context);

	cairo_restore(context);
}

/* 256x256. */
static void draw_pattern2(cairo_t *context) {
	double x = 0, y = 0;
	cairo_pattern_t *pat;

	cairo_save(context);

	pat = cairo_pattern_create_linear(x, y, x, y + 256.0);
	cairo_pattern_add_color_stop_rgba(pat, 0, 0, 0, 0, 1);
	cairo_pattern_add_color_stop_rgba(pat, 1, 1, 1, 1, 1);
	cairo_rectangle(context, x, y, 256, 256);
	cairo_set_source(context, pat);
	cairo_fill(context);
	cairo_pattern_destroy(pat);

	pat = cairo_pattern_create_radial(x + 115.2, y + 102.4, 25.6, x + 102.4, y + 102.4, 128.0);
	cairo_pattern_add_color_stop_rgba(pat, 0, 1, 1, 1, 1);
	cairo_pattern_add_color_stop_rgba(pat, 1, 0, 0, 0, 1);
	cairo_set_source(context, pat);
	cairo_arc(context, x + 128.0, y + 128.0, 76.8, 0, 2 * M_PI);
	cairo_fill(context);
	cairo_pattern_destroy(pat);

	cairo_restore(context);
}

/* 600x450. */
static void draw_pattern3(cairo_t *context) {
	cairo_font_face_t *cfont;
	FT_Library freetype;
	FT_Face face;

	FT_Init_FreeType(&freetype);
	FT_New_Face(freetype, "/system/data/fonts/DejaVuSansMono.ttf", 0, &face);
	cfont = cairo_ft_font_face_create_for_ft_face(face, 0);

	cairo_save(context);

	cairo_rectangle(context, 0, 0, 600, 450);
	cairo_set_source_rgba(context, 0, 0, 0, 0.9);
	cairo_fill(context);

	cairo_set_source_rgb(context, 1, 1, 1);
	cairo_set_font_face(context, cfont);
	cairo_set_font_size(context, 12.0);
	cairo_move_to(context, 3, 13);
	cairo_show_text(context, "Hello, World!");

	cairo_restore(context);
}

class Window {
public:
	typedef void (*DrawFunc)(cairo_t *);

	Window(const char *title, int16_t x, int16_t y, uint16_t width, uint16_t height, DrawFunc func) :
		m_inited(false), m_width(width), m_height(height), m_func(func)
	{
		status_t ret;

		{
			Rect rect = { { x, y }, { width, height } };
			ret = g_conn->CreateWindow(rect, m_id);
			if(ret != STATUS_SUCCESS) {
				cout << "Failed to create window: " << ret << endl;
				return;
			}
		}

		g_conn->SetWindowTitle(m_id, title);

		ret = g_conn->GetWindowSurface(m_id, m_surface_id);
		if(ret != STATUS_SUCCESS) {
			cout << "Failed to get surface: " << ret << endl;
			return;
		}

		ret = area_open(m_surface_id, &m_surface_handle);
		if(ret != STATUS_SUCCESS) {
			cout << "Failed to open surface: " << ret << endl;
			return;
		}

		ret = vm_map(NULL, p2align(width * height * 4, 0x1000),
		             VM_MAP_READ | VM_MAP_WRITE, m_surface_handle,
		             0, &m_mapping);
		if(ret != STATUS_SUCCESS) {
			cout << "Failed to map surface: " << ret << endl;
			return;
		}

		m_surface = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char *>(m_mapping),
		                                                CAIRO_FORMAT_ARGB32, width, height, width * 4);
		if(cairo_surface_status(m_surface) != CAIRO_STATUS_SUCCESS) {
			cout << "Failed to create Cairo surface: " << cairo_status_to_string(cairo_surface_status(m_surface)) << endl;
			return;
		}
		m_context = cairo_create(m_surface);
		if(cairo_status(m_context) != CAIRO_STATUS_SUCCESS) {
			cout << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(m_context)) << endl;
			return;
		}

		m_func(m_context);

		{
			Rect rect = { { 0, 0 }, { width, height } };
			ret = g_conn->UpdateWindow(m_id, rect);
			if(ret != STATUS_SUCCESS) {
				cout << "Failed to update region: " << ret << endl;
				return;
			}
		}

		g_conn->ShowWindow(m_id);
		m_inited = true;
	}

	bool Inited() const { return m_inited; }

	void MoveTo(int16_t x, int16_t y) {
		Point pos = { x, y };
		g_conn->MoveWindow(m_id, pos);
	}
private:
	bool m_inited;
	WindowID m_id;
	int m_width;
	int m_height;
	DrawFunc m_func;
	area_id_t m_surface_id;
	handle_t m_surface_handle;
	void *m_mapping;
	cairo_t *m_context;
	cairo_surface_t *m_surface;
};

int main(int argc, char **argv) {
	kiwi::EventLoop loop;

	g_conn = new ServerConnection;
	Window window1("Test Window 1", 750, 350, 256, 256, draw_pattern2);
	if(!window1.Inited()) {
		return 1;
	}
	Window window2("Test Window 2", 600, 500, 300, 300, draw_pattern1);
	if(!window2.Inited()) {
		return 1;
	}
	Window window3("Console", 100, 100, 600, 450, draw_pattern3);
	if(!window3.Inited()) {
		return 1;
	}
#if 0
	int count = 0;
	int direction = 0;
	uint16_t x = 200, y = 100;
	while(true) {
		thread_usleep(5000, NULL);
		switch(direction) {
		case 0:
			x += 4;
			break;
		case 1:
			y += 4;
			break;
		case 2:
			x -= 4;
			break;
		case 3:
			y -= 4;
			break;
		}
		window2.MoveTo(x, y);
		if(++count == 25) {
			count = 0;
			direction++;
			direction %= 4;
		}
	}
#endif
	loop.Run();
	return 0;
}
