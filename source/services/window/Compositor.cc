/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Compositor class.
 */

#include <iostream>
#include <exception>

#include "Decoration.h"
#include "Display.h"
#include "ServerSurface.h"
#include "Compositor.h"

using namespace kiwi;
using namespace std;

/** Set up the compositor.
 * @param display	Display that windows should be rendered to.
 * @param root		Root window. */
Compositor::Compositor(Display *display, ServerWindow *root) :
	m_timer(Timer::kOneShotMode), m_display(display), m_root(root),
	m_surface(0), m_context(0)
{
	/* Set up the timer. */
	m_timer.OnTimer.Connect(this, &Compositor::PerformRedraw);

	/* Create a surface to render to. */
	m_surface = new ServerSurface(0, display->GetSize());

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
	Rect rect(0, 0, m_surface->GetWidth(), m_surface->GetHeight());
	m_redraw_region.Union(rect);
	ScheduleRedraw();
}

/** Redraw a rectangular area on screen.
 * @param rect		Rectangle to redraw. */
void Compositor::Redraw(Rect rect) {
	if(rect.IsValid()) {
		m_redraw_region.Union(rect);
		ScheduleRedraw();
	}
}

/** Redraw a screen region.
 * @param region	Region to redraw. */
void Compositor::Redraw(const Region &region) {
	/* Add the region to the redraw region and schedule a redraw. */
	if(!region.Empty()) {
		m_redraw_region.Union(region);
		ScheduleRedraw();
	}
}

/** Render to the back buffer.
 * @param window	Window to render.
 * @param off_x		X position offset.
 * @param off_y		Y position offset. */
void Compositor::Render(ServerWindow *window, int off_x, int off_y) {
	cairo_save(m_context);

	off_x += window->GetFrame().GetX();
	off_y += window->GetFrame().GetY();

	/* Paint decoration, if any. */
	Decoration *decor = window->GetDecoration();
	if(decor) {
		cairo_set_source_surface(m_context, decor->GetSurface(),
		                         off_x + decor->GetFrame().GetX(),
		                         off_y + decor->GetFrame().GetY());
		cairo_paint(m_context);
	}

	/* The surface size can differ from the window size if the window has
	 * been resized but the client has not updated the surface. Handle this
	 * possibility by clipping to the window size. */
	Rect frame = window->GetAbsoluteFrame();
	if(frame.GetSize() != window->GetSurface()->GetSize()) {
		cairo_rectangle(m_context, frame.GetX(), frame.GetY(), frame.GetWidth(), frame.GetHeight());
		cairo_clip(m_context);
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

/** Start the redraw timer. */
void Compositor::ScheduleRedraw() {
	if(!m_timer.IsRunning()) {
		/* Redraw every millisecond. */
		m_timer.Start(1000);
	}
}

/** Perform all queued redraw events. */
void Compositor::PerformRedraw() {
	/* Intersect the region with the screen area so we only have what is
	 * actually on screen. */
	Rect screen(0, 0, m_surface->GetWidth(), m_surface->GetHeight());
	m_redraw_region.Intersect(screen);

	/* Get the rectangles from the region and clear it. */
	Region::RectArray rects;
	m_redraw_region.GetRects(rects);
	m_redraw_region.Clear();

	/* If no rectangles were inside the screen area, do nothing. */
	if(rects.empty()) {
		return;
	}

	cairo_save(m_context);

	/* Add each rectangle to the current path. */
	for(auto it = rects.begin(); it != rects.end(); ++it) {
		cairo_rectangle(m_context, it->GetX(), it->GetY(), it->GetWidth(), it->GetHeight());
	}

	/* Set the clip region to the updated area. */
	cairo_clip(m_context);

	/* Render all of the windows into the back buffer. Only the affected
	 * areas will be updated. */
	Render(m_root, 0, 0);

	cairo_restore(m_context);

	/* Update the screen. */
	for(auto it = rects.begin(); it != rects.end(); ++it) {
		m_display->DrawSurface(m_surface, it->GetTopLeft(), it->GetTopLeft(), it->GetSize());
	}
}
