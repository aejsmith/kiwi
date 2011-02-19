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
 * @brief		Point class.
 */

#ifndef __KIWI_GRAPHICS_POINT_H
#define __KIWI_GRAPHICS_POINT_H

#include <kiwi/CoreDefs.h>

namespace kiwi {

/** Class representing a single point on a 2D plane. */
class KIWI_PUBLIC Point {
public:
	/** Initialise the point to (0, 0). */
	Point() : m_x(0), m_y(0) {}

	/** Initialise the point.
	 * @param x		X position of the point.
	 * @param y		Y position of the point. */
	Point(int x, int y) : m_x(x), m_y(y) {}

	/** Add another point to the point.
	 * @param other		Other point. Its X and Y coordinates will be
	 *			added to this point's coordinates.
	 * @return		Result of the addition. */
	Point operator +(Point other) const {
		return Point(m_x + other.m_x, m_y + other.m_y);
	}

	/** Subtract another point from the point.
	 * @param other		Other point. Its X and Y coordinates will be
	 *			subtracted from this point's coordinates.
	 * @return		Result of the subtraction. */
	Point operator -(Point other) const {
		return Point(m_x - other.m_x, m_y - other.m_y);
	}

	/** Add another point to the point.
	 * @param other		Other point. Its X and Y coordinates will be
	 *			added to this point's coordinates. */
	Point &operator +=(Point other) {
		m_x += other.m_x;
		m_y += other.m_y;
		return *this;
	}

	/** Subtract another point from the point.
	 * @param other		Other point. Its X and Y coordinates will be
	 *			subtracted from this point's coordinates. */
	Point &operator -=(Point other) {
		m_x -= other.m_x;
		m_y -= other.m_y;
		return *this;
	}

	/** Move the point.
	 * @param dx		X delta.
	 * @param dy		Y delta. */
	void Translate(int dx, int dy) {
		m_x += dx;
		m_y += dy;
	}

	/** Move the point.
	 * @param dx		X delta.
	 * @param dy		Y delta.
	 * @return		Point with translation applied. */
	Point Translated(int dx, int dy) const {
		return Point(m_x + dx, m_y + dy);
	}

	/** Get X position of point.
	 * @return		X position. */
	int GetX() const { return m_x; }

	/** Get Y position of point.
	 * @return		Y position. */
	int GetY() const { return m_y; }
private:
	int m_x;			/**< X position. */
	int m_y;			/**< Y position. */
};

}

#endif /* __KIWI_GRAPHICS_POINT_H */
