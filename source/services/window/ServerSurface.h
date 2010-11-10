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
 * @brief		Surface class.
 */

#ifndef __SERVERSURFACE_H
#define __SERVERSURFACE_H

#include <cairo/cairo.h>

#include <drivers/display.h>

#include <kernel/types.h>

#include <kiwi/Graphics/Size.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Object.h>

#include <pixman.h>

class Connection;

/** Class implementing the server side of kiwi::Surface. */
class ServerSurface : public kiwi::Object, kiwi::Noncopyable {
public:
	ServerSurface(Connection *owner, kiwi::Size size);
	~ServerSurface();

	area_id_t GetID() const;
	void *GetData();
	size_t GetDataSize() const;
	pixman_image_t *GetPixmanImage();
	cairo_surface_t *GetCairoSurface();
	status_t Resize(kiwi::Size size);

	/** Get the owner of the surface.
	 * @return		Connection that owns the surface. */
	Connection *GetOwner() const { return m_owner; }

	/** Get the surface's size.
	 * @return		Size of the surface. */
	kiwi::Size GetSize() const { return m_size; }

	/** Get the surface's width.
	 * @return		Width of the surface. */
	int GetWidth() const { return m_size.GetWidth(); }

	/** Get the surface's height.
	 * @return		Height of the surface. */
	int GetHeight() const { return m_size.GetHeight(); }
private:
	status_t Map();
	void Unmap();

	Connection *m_owner;		/**< Connection that owns the surface. */
	kiwi::Size m_size;		/**< Size of the surface. */
	handle_t m_area;		/**< Handle to the surface's area. */
	void *m_mapping;		/**< Mapping for the surface area. */
	pixman_image_t *m_image;	/**< Pixman image for the surface data. */
	cairo_surface_t *m_cairo;	/**< Cairo surface for operating on the surface. */
};

#endif /* __SERVERSURFACE_H */
