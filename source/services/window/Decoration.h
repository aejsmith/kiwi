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
 * @brief		Window decoration class.
 */

#ifndef __DECORATION_H
#define __DECORATION_H

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>
#include <kiwi/Support/Noncopyable.h>

#include "MouseReceiver.h"

class ServerWindow;

/** Class providing a decoration for a window. */
class Decoration : public MouseReceiver, kiwi::Noncopyable {
public:
	Decoration(ServerWindow *window);
	~Decoration();

	void Update();
	void MouseMoved(const kiwi::MouseEvent &event);
	void MousePressed(const kiwi::MouseEvent &event);
	void MouseReleased(const kiwi::MouseEvent &event);
	kiwi::Point RelativePoint(kiwi::Point pos) const;

	/** Get the Cairo surface for the decoration.
	 * @return		Pointer to Cairo surface. */
	cairo_surface_t *GetSurface() const { return m_surface; }

	/** Get the decoration's area.
	 * @return		Rectangle for the decoration's area, relative
	 *			to the window that the decoration is for. */
	kiwi::Rect GetFrame() const { return m_frame; }
private:
	ServerWindow *m_window;		/**< Window that the decoration is for. */
	kiwi::Rect m_frame;		/**< Area covered by the decoration, relative to window. */
	cairo_surface_t *m_surface;	/**< Surface containing the rendered decoration. */
	bool m_grabbed;			/**< Whether the decoration is grabbed. */
	kiwi::Point m_grab_pos;		/**< Grab position. */
};

#endif /* __DECORATION_H */
