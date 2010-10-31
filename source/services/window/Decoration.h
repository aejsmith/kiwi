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

#ifndef __DECORATION_H
#define __DECORATION_H

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>
#include <kiwi/Support/Noncopyable.h>

class Window;

/** Class providing a decoration for a window. */
class Decoration : kiwi::Noncopyable {
public:
	Decoration(Window *window);
	~Decoration();

	void Redraw();

	/** Get the Cairo surface for the decoration.
	 * @return		Pointer to Cairo surface. */
	cairo_surface_t *GetSurface() const { return m_surface; }

	/** Get the decoration's area.
	 * @return		Rectangle for the decoration's area, relative
	 *			to the window that the decoration is for. */
	kiwi::Rect GetRect() const { return m_rect; }
private:
	Window *m_window;		/**< Window that the decoration is for. */
	kiwi::Rect m_rect;		/**< Area covered by the decoration, relative to window. */
	cairo_surface_t *m_surface;	/**< Surface containing the rendered decoration. */
};

#endif /* __DECORATION_H */
