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
 * @brief		Region class.
 */

#ifndef __KIWI_GRAPHICS_REGION_H
#define __KIWI_GRAPHICS_REGION_H

#include <kiwi/Graphics/Point.h>
#include <kiwi/Graphics/Rect.h>

#include <vector>

namespace kiwi {

/** Class representing a region made up of multiple rectangles. */
class KIWI_PUBLIC Region {
public:
	/** Rectangle array type.
	 * @fixme		Replace! */
	typedef std::vector<Rect> RectArray;

	Region();
	Region(const Rect &rect);
	Region(const Region &other);
	~Region();

	Region &operator =(const Region &other);
	bool operator ==(const Region &other) const;
	bool operator !=(const Region &other) const;

	void GetRects(RectArray &array) const;

	bool Empty() const;
	bool Contains(const Point &point) const;

	void Union(const Region &other);
	void Union(const Rect &rect);
	void Intersect(const Region &other);
	void Intersect(const Rect &rect);
	void Subtract(const Region &other);
	void Subtract(const Rect &rect);
	void XOR(const Region &other);
	void XOR(const Rect &rect);
private:
	void *m_data;			/**< Pointer to Cairo region. */
};

}

#endif /* __KIWI_GRAPHICS_REGION_H */
