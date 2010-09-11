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
 *
 * The Surface class provides a buffer that can be used by clients to draw
 * onto. Internally, surfaces can have any format, however surfaces given to
 * clients are all 32-bit ARGB. The only surface which has a different format
 * is the surface representing the graphics framebuffer, which has the correct
 * format for the mode the display is in.
 */

#include <kernel/area.h>
#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/vm.h>

#include <kiwi/Error.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "Surface.h"

using namespace kiwi;
using namespace std;

/** Work out the bytes per pixel for a format.
 * @param format	Pixel format.
 * @return		Number of bytes per pixel. */
static size_t bytes_per_pixel(pixel_format_t format) {
	switch(format) {
	case PIXEL_FORMAT_ARGB32:
	case PIXEL_FORMAT_BGRA32:
	case PIXEL_FORMAT_RGB32:
	case PIXEL_FORMAT_BGR32:
		return 4;
	case PIXEL_FORMAT_RGB24:
	case PIXEL_FORMAT_BGR24:
		return 3;
	case PIXEL_FORMAT_ARGB16:
	case PIXEL_FORMAT_BGRA16:
	case PIXEL_FORMAT_RGB16:
	case PIXEL_FORMAT_BGR16:
		return 2;
	case PIXEL_FORMAT_RGB15:
	case PIXEL_FORMAT_BGR15:
		return 2;
	case PIXEL_FORMAT_IDX8:
	case PIXEL_FORMAT_GREY8:
		clog << "8-bit surfaces not implemented" << endl;
		throw OSError(STATUS_NOT_IMPLEMENTED);
	}

	assert(0 && "Invalid format passed to Surface::Surface()");
	return 0;
}

/** Work out the pixman format for a pixel format.
 * @param format	Pixel format.
 * @return		Corresponding pixman format. */
static pixman_format_code_t pixman_format_for_format(pixel_format_t format) {
	switch(format) {
	case PIXEL_FORMAT_ARGB32:
		return PIXMAN_a8r8g8b8;
	case PIXEL_FORMAT_BGRA32:
		return PIXMAN_b8g8r8a8;
	case PIXEL_FORMAT_RGB32:
		return PIXMAN_x8r8g8b8;
	case PIXEL_FORMAT_BGR32:
		return PIXMAN_b8g8r8x8;
	case PIXEL_FORMAT_RGB24:
		return PIXMAN_r8g8b8;
	case PIXEL_FORMAT_BGR24:
		return PIXMAN_b8g8r8;
	case PIXEL_FORMAT_ARGB16:
		return PIXMAN_a1r5g5b5;
	case PIXEL_FORMAT_RGB16:
		return PIXMAN_r5g6b5;
	case PIXEL_FORMAT_BGR16:
		return PIXMAN_b5g6r5;
	case PIXEL_FORMAT_RGB15:
		return PIXMAN_x1r5g5b5;
	case PIXEL_FORMAT_BGRA16:
	case PIXEL_FORMAT_BGR15:
		/* Eh? Pixman doesn't support these. */
		clog << "Unsupported format" << endl;
		throw OSError(STATUS_NOT_IMPLEMENTED);
	case PIXEL_FORMAT_IDX8:
	case PIXEL_FORMAT_GREY8:
		clog << "8-bit surfaces not implemented" << endl;
		throw OSError(STATUS_NOT_IMPLEMENTED);
	}

	assert(0 && "Invalid format passed to Surface::Surface()");
	abort();
}

/** Create a surface.
 * @param width		Width of the surface (in pixels).
 * @param height	Height of the surface (in pixels).
 * @param format	Format of a pixel in the surface. */
Surface::Surface(uint32_t width, uint32_t height, pixel_format_t format) :
	m_width(width), m_height(height), m_format(format), m_mapping(0),
	m_image(0)
{
	/* Get the size of the area to create. */
	size_t size = width * height * bytes_per_pixel(format);
	if(size % 0x1000) {
		size += 0x1000;
		size -= size % 0x1000;
	}

	/* Create a new area. */
	status_t ret = area_create(size, -1, 0, &m_area);
	if(ret != STATUS_SUCCESS) {
		OSError e(ret);
		clog << "Failed to create area for surface: " << e.GetDescription() << endl;
		throw e;
	}
}

/** Create a surface over a backing object.
 * @param obj		Handle to backing object.
 * @param offset	Offset into backing object.
 * @param width		Width of the surface (in pixels).
 * @param height	Height of the surface (in pixels).
 * @param format	Format of a pixel in the surface. */
Surface::Surface(handle_t obj, offset_t offset, uint32_t width, uint32_t height, pixel_format_t format) :
	m_width(width), m_height(height), m_format(format), m_mapping(0),
	m_image(0)
{
	/* Get the size of the area to create. */
	size_t size = GetDataSize();
	if(size % 0x1000) {
		size += 0x1000;
		size -= size % 0x1000;
	}

	/* Create a new area. */
	status_t ret = area_create(size, obj, offset, &m_area);
	if(ret != STATUS_SUCCESS) {
		OSError e(ret);
		clog << "Failed to create area for surface: " << e.GetDescription();
		throw e;
	}
}

/** Destroy the surface. */
Surface::~Surface() {
	Unmap();
	handle_close(m_area);
}

/** Get the ID of the surface (the same as its area ID).
 * @return		ID of the surface. */
area_id_t Surface::GetID() const {
	return area_id(m_area);
}

/** Get a pointer to the surface's data.
 * @return		Pointer to surface data. */
void *Surface::GetData() {
	if(Map() != STATUS_SUCCESS) {
		return NULL;
	}
	return m_mapping;
}

/** Get the size of the surface's data.
 * @return		Size of the surface's data. */
size_t Surface::GetDataSize() const {
	return (m_width * m_height * bytes_per_pixel(m_format));
}

/** Change the size of the surface.
 * @param width		New width (in pixels).
 * @param height	New height (in pixels).
 * @return		Status code describing result of the operation. */
status_t Surface::Resize(uint32_t width, uint32_t height) {
	Unmap();

	/* Get the new size for the area. */
	size_t size = width * height * bytes_per_pixel(m_format);
	if(size % 0x1000) {
		size += 0x1000;
		size -= size % 0x1000;
	}

	/* Resize the area. */
	status_t ret = area_resize(m_area, size);
	if(ret != STATUS_SUCCESS) {
		// TODO: Workaround for kernel not supporting shrinking.
		if(ret != STATUS_NOT_IMPLEMENTED) {
			return ret;
		}
	}

	m_width = width;
	m_height = height;
	return STATUS_SUCCESS;
}

/** Copy the surface to another surface.
 * @param dest		Destination surface.
 * @param src_x		X position to copy from.
 * @param src_y		Y position to copy from.
 * @param dest_x	X position on destination to copy to.
 * @param dest_y	Y position on destination to copy to.
 * @param width		Width of area to copy.
 * @param height	Height of area to copy. */
void Surface::Copy(Surface *dest, uint32_t src_x, uint32_t src_y, uint32_t dest_x, uint32_t dest_y,
                   uint32_t width, uint32_t height) {
	/* Ensure that the images are mapped in. */
	status_t ret = Map();
	if(ret != STATUS_SUCCESS) {
		clog << "Could not map source surface for copy (" << ret << ')' << endl;
		return;
	}
	ret = dest->Map();
	if(ret != STATUS_SUCCESS) {
		clog << "Could not map destination surface for copy (" << ret << ')' << endl;
		return;
	}

	/* Pixman handles sanitising all parameters. Use the source operator
	 * as we just want to stick the source surface over the destination.
	 * Any compositing is done by the window manager. */
	pixman_image_composite(PIXMAN_OP_SRC, m_image, NULL, dest->m_image, src_x, src_y,
	                       0, 0, dest_x, dest_y, width, height);
}

/** Map the surface into memory.
 * @return		Status code describing result of the operation. */
status_t Surface::Map() {
	if(!m_mapping) {
		status_t ret = vm_map(NULL, area_size(m_area), VM_MAP_READ | VM_MAP_WRITE, m_area, 0, &m_mapping);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		m_image = pixman_image_create_bits(pixman_format_for_format(m_format),
		                                   m_width, m_height,
		                                   reinterpret_cast<uint32_t *>(m_mapping),
		                                   m_width * bytes_per_pixel(m_format));
		if(!m_image) {
			Unmap();
			return STATUS_NO_MEMORY;
		}
	}
	return STATUS_SUCCESS;
}

/** Unmap the surface. */
void Surface::Unmap() {
	if(m_image) {
		pixman_image_unref(m_image);
		m_image = 0;
	}
	if(m_mapping) {
		vm_unmap(m_mapping, area_size(m_area));
		m_mapping = 0;
	}
}
