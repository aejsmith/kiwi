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
 * @brief		Compositor class.
 */

#include <iostream>
#include <exception>

#include "Decoration.h"
#include "Display.h"
#include "Surface.h"
#include "Compositor.h"

using namespace kiwi;
using namespace std;

/** Set up the compositor.
 * @param display	Display that windows should be rendered to.
 * @param root		Root window. */
Compositor::Compositor(Display *display, Window *root) :
	m_display(display), m_root(root), m_surface(0), m_context(0)
{
	/* Create a surface to render to. */
	m_surface = new Surface(display->GetCurrentMode().width, display->GetCurrentMode().height);

	/* Set up a Cairo context for rendering on to the surface. */
	m_context = cairo_create(m_surface->GetCairoSurface());
	if(cairo_status(m_context) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(m_context)) << endl;
		throw exception();
	}
}

/** Destroy the compositor. */
Compositor::~Compositor() {
	cairo_destroy(m_context);
	delete m_surface;
}

/** Redraw the entire screen. */
void Compositor::Redraw() {
	/* Re-render the entire surface. */
	Render();

	/* Update the screen. */
	m_display->DrawSurface(m_surface, 0, 0, 0, 0, m_surface->GetWidth(), m_surface->GetHeight());
}

/** Redraw a rectangular area on screen.
 * @param rect		Rectangle to redraw. */
void Compositor::Redraw(const Rect &rect) {
	if(!rect.IsValid()) {
		return;
	}

	/* Get the area to draw. */
	Rect area(0, 0, m_surface->GetWidth(), m_surface->GetHeight());
	area.Intersect(rect);
	if(area.IsValid()) {
		/* Set the clip region so we only update the area changed. */
		cairo_save(m_context);
		cairo_rectangle(m_context, area.GetX(), area.GetY(), area.GetWidth(), area.GetHeight());
		cairo_clip(m_context);

		/* Redraw. */
		Render();
		cairo_restore(m_context);

		/* Update the screen. */
		m_display->DrawSurface(m_surface, area.GetX(), area.GetY(), area.GetX(),
		                       area.GetY(), area.GetWidth(), area.GetHeight());
	}
}

/** Redraw a screen region.
 * @param region	Region to redraw. */
void Compositor::Redraw(const Region &region) {
	Region::RectArray rects;
	region.GetRects(rects);

	cairo_save(m_context);

	/* Add each rectangle to the current path. */
	Rect screen(0, 0, m_surface->GetWidth(), m_surface->GetHeight());
	for(auto it = rects.begin(); it != rects.end(); ) {
		it->Intersect(screen);
		if(it->IsValid()) {
			cairo_rectangle(m_context, it->GetX(), it->GetY(), it->GetWidth(), it->GetHeight());
			++it;
		} else {
			it = rects.erase(it);
		}
	}

	/* If no rectangles were inside the screen area, do nothing. */
	if(rects.empty()) {
		return;
	}

	/* Set the clip region to the updated area. */
	cairo_clip(m_context);

	/* Redraw. */
	Render();
	cairo_restore(m_context);

	/* Update the screen. */
	for(auto it = rects.begin(); it != rects.end(); ++it) {
		m_display->DrawSurface(m_surface, it->GetX(), it->GetY(), it->GetX(),
		                       it->GetY(), it->GetWidth(), it->GetHeight());
	}
}

/** Render to the back buffer.
 * @param window	Window to render.
 * @param off_x		X position offset.
 * @param off_y		Y position offset. */
void Compositor::Render(Window *window, int16_t off_x, int16_t off_y) {
	cairo_save(m_context);

	off_x += window->GetRect().GetX();
	off_y += window->GetRect().GetY();

	/* Paint decoration, if any. */
	Decoration *decor = window->GetDecoration();
	if(decor) {
		cairo_set_source_surface(m_context, decor->GetSurface(),
		                         off_x + decor->GetRect().GetX(),
		                         off_y + decor->GetRect().GetY());
		cairo_paint(m_context);
	}

	/* Paint main window content. */
	cairo_set_source_surface(m_context, window->GetSurface()->GetCairoSurface(), off_x, off_y);
	cairo_paint(m_context);

	/* Draw the child windows. */
	for(auto it = window->GetChildren().Begin(); it != window->GetChildren().End(); ++it) {
		Render(*it, off_x, off_y);
	}

	cairo_restore(m_context);
}
