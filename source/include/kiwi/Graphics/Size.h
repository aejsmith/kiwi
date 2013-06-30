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
 * @brief		Size class.
 */

#ifndef __KIWI_GRAPHICS_SIZE_H
#define __KIWI_GRAPHICS_SIZE_H

#include <kiwi/CoreDefs.h>

namespace kiwi {

/** Class representing a rectangle size. */
class KIWI_PUBLIC Size {
public:
	/** Initialise the object to be an invalid size (0 x 0). */
	Size() : m_width(0), m_height(0) {}

	/** Initialise the size.
	 * @param width		Width.
	 * @param height	Height. */
	Size(int width, int height) : m_width(width), m_height(height) {}

	/** Check whether the size is equal to another size.
	 * @param other		Size to check against.
	 * @return		Whether equal. */
	bool operator ==(Size other) const {
		return (m_width == other.m_width && m_height == other.m_height);
	}

	/** Check whether the size is different from another size.
	 * @param other		Size to check against.
	 * @return		Whether different. */
	bool operator !=(Size other) const {
		return (m_width != other.m_width || m_height != other.m_height);
	}

	/** Check whether the size is valid (width and height greater than 0).
	 * @return		Whether valid. */
	bool IsValid() const {
		return (m_width > 0 && m_height > 0);
	}

	/** Get the width.
	 * @return		Width. */
	int GetWidth() const { return m_width; }

	/** Set the width.
	 * @param width		New width. */
	void SetWidth(int width) { m_width = width; }

	/** Get the height.
	 * @return		Height. */
	int GetHeight() const { return m_height; }

	/** Set the height.
	 * @param height	New height. */
	void SetHeight(int height) { m_height = height; }
private:
	int m_width;			/**< Width. */
	int m_height;			/**< Height. */
};

}

#endif /* __KIWI_GRAPHICS_SIZE_H */
