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
	Region(Rect rect);
	Region(const Region &other);
	~Region();

	Region &operator =(const Region &other);
	bool operator ==(const Region &other) const;
	bool operator !=(const Region &other) const;

	void GetRects(RectArray &array) const;

	bool Empty() const;
	bool Contains(Point point) const;

	void Clear();
	void Union(const Region &other);
	void Union(Rect rect);
	void Intersect(const Region &other);
	void Intersect(Rect rect);
	void Subtract(const Region &other);
	void Subtract(Rect rect);
	void XOR(const Region &other);
	void XOR(Rect rect);
private:
	void *m_data;			/**< Pointer to Cairo region. */
};

}

#endif /* __KIWI_GRAPHICS_REGION_H */
