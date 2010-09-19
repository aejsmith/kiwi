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
 * @brief		Rectangle class.
 */

#ifndef __KIWI_RECT_H
#define __KIWI_RECT_H

#include <kiwi/Point.h>
#include <algorithm>

namespace kiwi {

/** Class representing a rectangle area. */
class Rect {
public:
	Rect() : m_left(0), m_top(0), m_right(0), m_bottom(0) {}

	/** Initialise the rectangle.
	 * @param x		X position of the top left of the rectangle.
	 * @param y		Y position of the top left of the rectangle.
	 * @param width		Width of the rectangle.
	 * @param height	Height of the rectangle. */
	Rect(int x, int y, int width, int height) :
		m_left(x), m_top(y), m_right(x + width), m_bottom(y + height)
	{
		if(width < 0) { m_right = m_left; }
		if(height < 0) { m_bottom = m_top; }
	}

	/** Initialise the rectangle.
	 * @param p1		Point for top left of the rectangle.
	 * @param p2		Point for bottom right of the rectangle. */
	Rect(const Point &p1, const Point &p2) :
		m_left(p1.GetX()), m_top(p1.GetY()), m_right(p2.GetX()),
		m_bottom(p2.GetY())
	{
		if(m_right < m_left) { m_right = m_left; }
		if(m_bottom < m_top) { m_bottom = m_top; }
	}

	/** Get X position of top left of rectangle.
	 * @return		X position. */
	int GetX() const { return m_left; }

	/** Get Y position of top left of rectangle.
	 * @return		Y position. */
	int GetY() const { return m_top; }

	/** Get width of rectangle.
	 * @return		Width of rectangle. */
	int GetWidth() const { return m_right - m_left; }

	/** Get height of rectangle.
	 * @return		Height of rectangle. */
	int GetHeight() const { return m_bottom - m_top; }

	/** Get point for top left of rectangle.
	 * @return		Point for top left. */
	Point GetTopLeft() const { return Point(m_left, m_top); }

	/** Get point for bottom right of rectangle.
	 * @return		Point for bottom right. */
	Point GetBottomRight() const { return Point(m_right, m_bottom); }

	/** Check whether the rectangle is valid.
	 * @return		Whether the rectangle is valid. */
	bool IsValid() const {
		return (GetWidth() > 0 && GetHeight() > 0);
	}

	/** Check whether a point lies within the rectangle.
	 * @param point		Point to check. */
	bool Contains(const Point &point) const {
		if(point.GetX() >= m_left && point.GetX() < m_right &&
		   point.GetY() >= m_top && point.GetY() < m_bottom) {
			return true;
		} else {
			return false;
		}
	}

	/** Get the area where the rectangle intersects with another.
	 * @param other		Rectangle to intersect with.
	 * @return		Intersection rectangle. */
	Rect Intersect(const Rect &rect) const {
		Point tl(std::max(m_left, rect.m_left), std::max(m_top, rect.m_top));
		Point br(std::min(m_right, rect.m_right), std::min(m_bottom, rect.m_bottom));
		return Rect(tl, br);
	}

	/** Adjust the rectangle size.
	 * @param dx1		Value to add to top left X position.
	 * @param dy1		Value to add to top left Y position.
	 * @param dx2		Value to add to bottom right X position.
	 * @param dy2		Value to add to bottom right Y position. */
	void Adjust(int dx1, int dy1, int dx2, int dy2) {
		m_left += dx1;
		m_top += dy1;
		m_right += dx2;
		m_bottom += dy2;
	}

	/** Move the rectangle.
	 * @param pos		New position. */
	void MoveTo(const Point &pos) {
		m_right = pos.GetX() + GetWidth();
		m_bottom = pos.GetY() + GetHeight();
		m_left = pos.GetX();
		m_top = pos.GetY();
	}
private:
	int m_left;			/**< X position of top left. */
	int m_top;			/**< Y position of top left. */
	int m_right;			/**< X position of bottom right. */
	int m_bottom;			/**< Y position of bottom right. */
};

}

#endif /* __KIWI_RECT_H */
