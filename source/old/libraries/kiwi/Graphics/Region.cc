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
 * @brief		Region class.
 *
 * Basically we cheat here and use Cairo internally - this is just a wrapper
 * around the Cairo region interface.
 *
 * @fixme		Handle Cairo errors.
 */

#include <cairo/cairo.h>
#include <kiwi/Graphics/Region.h>
#include <algorithm>

using namespace kiwi;
using namespace std;

/** Macro to get the Cairo region.
 * @param x		Pointer to Region to get from. */
#define CAIRO_REGION(x)		(reinterpret_cast<cairo_region_t *>((x)->m_data))

/** Macro to get the Cairo region.
 * @param x		Pointer to Region to get from. */
#define CAIRO_REGION_C(x)	(reinterpret_cast<const cairo_region_t *>((x)->m_data))

/** Macro to convert a Rect to a cairo_rectangle_int_t. */
#define CAIRO_RECT(name, rect)	\
	cairo_rectangle_int_t name = { (rect).GetX(), (rect).GetY(), (rect).GetWidth(), (rect).GetHeight() }

/** Construct an empty region. */
Region::Region() {
	m_data = cairo_region_create();
}

/** Construct a region containing a single rectangle.
 * @param rect		Rectangle to add to region. */
Region::Region(Rect rect) {
	CAIRO_RECT(_rect, rect);
	m_data = cairo_region_create_rectangle(&_rect);
}

/** Create a copy of another region.
 * @param other		Region to copy. */
Region::Region(const Region &other) {
	m_data = cairo_region_copy(CAIRO_REGION_C(&other));
}

/** Destroy the region. */
Region::~Region() {
	cairo_region_destroy(CAIRO_REGION(this));
}

/** Create a copy of another region.
 * @param other		Region to copy. */
Region &Region::operator =(const Region &other) {
	void *copy = cairo_region_copy(CAIRO_REGION_C(&other));
	swap(copy, m_data);
	cairo_region_destroy(reinterpret_cast<cairo_region_t *>(copy));
	return *this;
}

/** Compare the region with another.
 * @param other		Region to compare with.
 * @return		Whether the regions are equal. */
bool Region::operator ==(const Region &other) const {
	return cairo_region_equal(CAIRO_REGION_C(this), CAIRO_REGION_C(&other));
}

/** Compare the region with another.
 * @param other		Region to compare with.
 * @return		Whether the regions are not equal. */
bool Region::operator !=(const Region &other) const {
	return !(*this == other);
}

/** Get all of the rectangles in the region.
 * @param array		Array to fill in. Existing content will be cleared. */
void Region::GetRects(RectArray &array) const {
	array.clear();
	for(int i = 0; i < cairo_region_num_rectangles(CAIRO_REGION_C(this)); i++) {
		cairo_rectangle_int_t _tmp;
		cairo_region_get_rectangle(CAIRO_REGION_C(this), i, &_tmp);

		Rect rect(_tmp.x, _tmp.y, _tmp.width, _tmp.height);
		array.push_back(rect);
	}
}

/** Check whether the region is empty.
 * @return		Whether the region is empty. */
bool Region::Empty() const {
	return cairo_region_is_empty(CAIRO_REGION_C(this));
}

/** Check whether the region contains a point.
 * @param point		Point to check.
 * @return		Whether the region contains the point. */
bool Region::Contains(Point point) const {
	return cairo_region_contains_point(CAIRO_REGION_C(this), point.GetX(), point.GetY());
}

/** Clear the region. */
void Region::Clear() {
	void *region = cairo_region_create();
	swap(region, m_data);
	cairo_region_destroy(reinterpret_cast<cairo_region_t *>(region));
}

/** Set the area to the union of the current area and another region.
 * @param other		Other region. */
void Region::Union(const Region &other) {
	cairo_region_union(CAIRO_REGION(this), CAIRO_REGION_C(&other));
}

/** Set the area to the union of the current area and another rectangle.
 * @param rect		Other rectangle. */
void Region::Union(Rect rect) {
	CAIRO_RECT(_rect, rect);
	cairo_region_union_rectangle(CAIRO_REGION(this), &_rect);
}

/** Set the area to the intersection of the current area and another region.
 * @param other		Other region. */
void Region::Intersect(const Region &other) {
	cairo_region_intersect(CAIRO_REGION(this), CAIRO_REGION_C(&other));
}

/** Set the area to the intersection of the current area and another rectangle.
 * @param rect		Other rectangle. */
void Region::Intersect(Rect rect) {
	CAIRO_RECT(_rect, rect);
	cairo_region_intersect_rectangle(CAIRO_REGION(this), &_rect);
}

/** Subtract another region from the area.
 * @param other		Other region. */
void Region::Subtract(const Region &other) {
	cairo_region_subtract(CAIRO_REGION(this), CAIRO_REGION_C(&other));
}

/** Subtract another rectangle from the area.
 * @param rect		Other rectangle. */
void Region::Subtract(Rect rect) {
	CAIRO_RECT(_rect, rect);
	cairo_region_subtract_rectangle(CAIRO_REGION(this), &_rect);
}

/** Set the area to the exclusive-OR of the current area and another region.
 * @param other		Other region. */
void Region::XOR(const Region &other) {
	cairo_region_xor(CAIRO_REGION(this), CAIRO_REGION_C(&other));
}

/** Set the area to the exclusive-OR of the current area and another rectangle.
 * @param other		Other rectangle. */
void Region::XOR(Rect rect) {
	CAIRO_RECT(_rect, rect);
	cairo_region_xor_rectangle(CAIRO_REGION(this), &_rect);
}
