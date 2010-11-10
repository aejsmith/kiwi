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
