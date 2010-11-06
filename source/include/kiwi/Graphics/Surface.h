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
	Surface(const Size &size);
	Surface(area_id_t area);
	~Surface();

	Size GetSize() const;
	bool Resize(const Size &size);
	unsigned char *GetData();
	size_t GetDataSize() const;
	cairo_surface_t *GetCairoSurface();
private:
	SurfacePrivate *m_priv;		/**< Internal data for the surface. */
};

}

#endif /* __KIWI_GRAPHICS_SURFACE_H */
