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

#include <kiwi/Graphics/Rect.h>
#include <algorithm>

using namespace kiwi;

/** Check whether the rectangle is valid.
 * @return		Whether the rectangle is valid. */
bool Rect::IsValid() const {
	return (GetWidth() > 0 && GetHeight() > 0);
}

/** Check whether a point lies within the rectangle.
 * @param point		Point to check. */
bool Rect::Contains(const Point &point) const {
	if(point.GetX() >= m_left && point.GetX() < m_right &&
	   point.GetY() >= m_top && point.GetY() < m_bottom) {
		return true;
	} else {
		return false;
	}
}

/** Check whether the rectangle intersects with another.
 * @return rect		Rectangle to check.
 * @return		Whether the rectangles intersect. */
bool Rect::Intersects(const Rect &rect) const {
	return Intersected(rect).IsValid();
}

/** Intersect the rectangle with another.
 * @param rect		Rectangle to intersect with. */
void Rect::Intersect(const Rect &rect) {
	m_left = std::max(m_left, rect.m_left);
	m_top = std::max(m_top, rect.m_top);
	m_right = std::min(m_right, rect.m_right);
	m_bottom = std::min(m_bottom, rect.m_bottom);
}

/** Get the area where the rectangle intersects with another.
 * @param rect		Rectangle to intersect with.
 * @return		Area where the rectangles intersect. */
Rect Rect::Intersected(const Rect &rect) const {
	Point tl(std::max(m_left, rect.m_left), std::max(m_top, rect.m_top));
	Point br(std::min(m_right, rect.m_right), std::min(m_bottom, rect.m_bottom));
	return Rect(tl, br);
}

/** Adjust the rectangle coordinates.
 * @param dx1		Value to add to top left X position.
 * @param dy1		Value to add to top left Y position.
 * @param dx2		Value to add to bottom right X position.
 * @param dy2		Value to add to bottom right Y position. */
void Rect::Adjust(int dx1, int dy1, int dx2, int dy2) {
	m_left += dx1;
	m_top += dy1;
	m_right += dx2;
	m_bottom += dy2;
}

/** Get a new rectangle with adjusted coordinates.
 * @param dx1		Value to add to top left X position.
 * @param dy1		Value to add to top left Y position.
 * @param dx2		Value to add to bottom right X position.
 * @param dy2		Value to add to bottom right Y position.
 * @return		Rectangle with adjustments made. */
Rect Rect::Adjusted(int dx1, int dy1, int dx2, int dy2) const {
	Point tl(m_left + dx1, m_top + dy1);
	Point br(m_right + dx2, m_bottom + dy2);
	return Rect(tl, br);
}

/** Translate the rectangle.
 * @param dx		Value to add to X position.
 * @param dy		Value to add to Y position. */
void Rect::Translate(int dx, int dy) {
	Adjust(dx, dy, dx, dy);
}

/** Get a new translated rectangle.
 * @param dx		Value to add to X position.
 * @param dy		Value to add to Y position.
 * @return		Rectangle with translation applied. */
Rect Rect::Translated(int dx, int dy) const {
	return Adjusted(dx, dy, dx, dy);
}

/** Move the rectangle.
 * @param x		X position to move to.
 * @param y		Y position to move to. */
void Rect::MoveTo(int x, int y) {
	return MoveTo(Point(x, y));
}

/** Move the rectangle.
 * @param pos		New position. */
void Rect::MoveTo(const Point &pos) {
	m_right = pos.GetX() + GetWidth();
	m_bottom = pos.GetY() + GetHeight();
	m_left = pos.GetX();
	m_top = pos.GetY();
}
