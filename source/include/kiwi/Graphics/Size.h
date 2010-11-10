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
