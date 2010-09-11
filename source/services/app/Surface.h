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

#ifndef __SURFACE_H
#define __SURFACE_H

#include <drivers/display.h>
#include <kernel/types.h>
#include <kiwi/Object.h>
#include <pixman.h>

/** Class representing a surface. */
class Surface : public kiwi::Object {
public:
	Surface(uint32_t width, uint32_t height, pixel_format_t format = PIXEL_FORMAT_ARGB32);
	Surface(handle_t obj, offset_t offset, uint32_t width, uint32_t height, pixel_format_t format);
	~Surface();

	/** Get the surface's width.
	 * @return		Width of the surface. */
	uint32_t GetWidth() const { return m_width; }

	/** Get the surface's height.
	 * @return		Height of the surface. */
	uint32_t GetHeight() const { return m_height; }

	area_id_t GetID() const;
	void *GetData();
	size_t GetDataSize() const;
	status_t Resize(uint32_t width, uint32_t height);
	//status_t Reconfigure(handle_t obj, offset_t offset, uint32_t width, uint32_t height, pixel_format_t format);
	void Copy(Surface *dest, uint32_t src_x, uint32_t src_y, uint32_t dest_x, uint32_t dest_y,
	          uint32_t width, uint32_t height);
private:
	status_t Map();
	void Unmap();

	handle_t m_area;		/**< Handle to the surface's area. */
	uint32_t m_width;		/**< Width of the surface. */
	uint32_t m_height;		/**< Height of the surface. */
	pixel_format_t m_format;	/**< Pixel format. */
	void *m_mapping;		/**< Mapping for the surface area. */
	pixman_image_t *m_image;	/**< Pixman image used for operations on the surface. */
};

#endif /* __SURFACE_H */
