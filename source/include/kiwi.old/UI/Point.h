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

#ifndef __KIWI_UI_POINT_H
#define __KIWI_UI_POINT_H

namespace kiwi {

/** Class representing a single point on a 2D plane. */
class Point {
public:
	/** Initialise the point.
	 * @param x		X position of the point.
	 * @param y		Y position of the point. */
	Point(int x, int y) : m_x(x), m_y(y) {}

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

#endif /* __KIWI_UI_POINT_H */
