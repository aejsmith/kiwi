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
 * @brief		Compositor class.
 */

#ifndef __COMPOSITOR_H
#define __COMPOSITOR_H

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>
#include <kiwi/Graphics/Region.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Timer.h>

#include <list>

#include "ServerWindow.h"

class Display;
class ServerSurface;

/** Class that manages the rendering of windows. */
class Compositor : public kiwi::Object, kiwi::Noncopyable {
public:
	Compositor(Display *display, ServerWindow *root);
	~Compositor();

	void Redraw();
	void Redraw(kiwi::Rect rect);
	void Redraw(const kiwi::Region &region);
private:
	void Render(ServerWindow *window, int off_x, int off_y);

	void ScheduleRedraw();
	void PerformRedraw();

	kiwi::Timer m_timer;		/**< Redraw timer. */
	kiwi::Region m_redraw_region;	/**< Redraw region. */
	Display *m_display;		/**< Display to render to. */
	ServerWindow *m_root;		/**< Root window. */
	ServerSurface *m_surface;	/**< Back buffer that rendering takes place on. */
	cairo_t *m_context;		/**< Cairo context for rendering. */
};

#endif /* __COMPOSITOR_H */
