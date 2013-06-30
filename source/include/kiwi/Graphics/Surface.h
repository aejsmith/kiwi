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
 * @brief		Graphics surface class.
 */

#ifndef __KIWI_GRAPHICS_SURFACE_H
#define __KIWI_GRAPHICS_SURFACE_H

#include <cairo/cairo.h>

#include <kiwi/Graphics/Size.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Object.h>

namespace kiwi {

class SurfacePrivate;

/** Class providing a surface to draw to.
 *
 * This class provides an area of memory which can be drawn to. Surfaces are
 * stored as 32-bit ARGB (4 bytes per pixel).
 */
class KIWI_PUBLIC Surface : public kiwi::Object, kiwi::Noncopyable {
public:
	Surface(Size size);
	Surface(area_id_t area);
	~Surface();

	Size GetSize() const;
	bool Resize(Size size);
	uint32_t *GetData();
	size_t GetDataSize() const;
	cairo_surface_t *GetCairoSurface();
private:
	SurfacePrivate *m_priv;		/**< Internal data for the surface. */
};

}

#endif /* __KIWI_GRAPHICS_SURFACE_H */
