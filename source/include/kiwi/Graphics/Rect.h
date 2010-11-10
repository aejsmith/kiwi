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

#ifndef __KIWI_GRAPHICS_RECT_H
#define __KIWI_GRAPHICS_RECT_H

#include <kiwi/Graphics/Point.h>
#include <kiwi/Graphics/Size.h>

namespace kiwi {

/** Class representing a rectangle area. */
class KIWI_PUBLIC Rect {
public:
	/** Initialise the rectangle with an invalid size. */
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
	Rect(Point p1, Point p2) :
		m_left(p1.GetX()), m_top(p1.GetY()), m_right(p2.GetX()),
		m_bottom(p2.GetY())
	{}

	/** Initialise the rectangle.
	 * @param pos		Position of the top left of the rectangle.
	 * @param size		Size of the rectangle. */
	Rect(Point pos, Size size) :
		m_left(pos.GetX()), m_top(pos.GetY()),
		m_right(pos.GetX() + size.GetWidth()),
		m_bottom(pos.GetY() + size.GetHeight())
	{}

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

	/** Get size of rectangle.
	 * @return		Size of the rectangle. */
	Size GetSize() const { return Size(GetWidth(), GetHeight()); }

	bool IsValid() const;
	bool Contains(Point point) const;
	bool Intersects(Rect rect) const;
	void Intersect(Rect rect);
	Rect Intersected(Rect rect) const;
	void Adjust(int dx1, int dy1, int dx2, int dy2);
	Rect Adjusted(int dx1, int dy1, int dx2, int dy2) const;
	void Translate(int dx, int dy);
	Rect Translated(int dx, int dy) const;
	void MoveTo(int x, int y);
	void MoveTo(Point pos);
	void Resize(int width, int height);
	void Resize(Size size);

	/** Intersect with another rectangle.
	 * @see			Intersected().
	 * @param rect		Rectangle to intersect with.
	 * @return		Intersected rectangle. */
	Rect operator &(Rect rect) const {
		return Intersected(rect);
	}

	/** Intersect with another rectangle.
	 * @see			Intersect().
	 * @param rect		Rectangle to intersect with. */
	Rect &operator &=(Rect rect) {
		Intersect(rect);
		return *this;
	}
private:
	int m_left;			/**< X position of top left. */
	int m_top;			/**< Y position of top left. */
	int m_right;			/**< X position of bottom right. */
	int m_bottom;			/**< Y position of bottom right. */
};

}

#endif /* __KIWI_GRAPHICS_RECT_H */
