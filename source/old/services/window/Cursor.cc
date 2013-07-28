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
 * @brief		Cursor class.
 *
 * This class implements the cursor, using a special window type. This window
 * type is kept above all other windows and cannot be made active. The window
 * is not published in the session, so clients are unable to do things to it
 * without going through the cursor calls.
 *
 * @todo		Different cursor types.
 */

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>

#include <cassert>
#include <exception>
#include <iostream>

#include "Cursor.h"
#include "Session.h"
#include "ServerSurface.h"
#include "ServerWindow.h"

using namespace kiwi;
using namespace std;

/** Properties of the cursor. */
static const char *kCursorPath = "/system/data/images/cursor.png";
static const int kCursorWidth = 24;
static const int kCursorHeight = 24;
static const int kCursorHotspotX = 6;
static const int kCursorHotspotY = 3;

/** Create the cursor.
 * @param session	Session that the cursor is for. */
Cursor::Cursor(Session *session) :
	m_session(session), m_root(session->GetRoot())
{
	cairo_surface_t *image;
	cairo_t *context;
	int x, y;

	/* Work out initial placement of the cursor (centre of screen). */
	x = (m_root->GetFrame().GetWidth() / 2) - (kCursorWidth / 2);
	y = (m_root->GetFrame().GetHeight() / 2) - (kCursorHeight / 2);

	/* Create the cursor window. */
	Rect frame(x, y, kCursorWidth, kCursorHeight);
	m_window = new ServerWindow(m_root->GetSession(), -1, m_root, 0, 0, BaseWindow::kCursorLevel, frame);

	/* Set up a Cairo context for rendering on to the cursor. */
	context = cairo_create(m_window->GetSurface()->GetCairoSurface());
	if(cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(context)) << endl;
		throw exception();
	}

	/* Load the image. */
	image = cairo_image_surface_create_from_png(kCursorPath);
	if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to load cursor image: ";
		clog << cairo_status_to_string(cairo_surface_status(image)) << endl;
		throw exception();
	}

	/* Check that the cursor is valid. */
	if(cairo_image_surface_get_width(image) != kCursorWidth ||
	   cairo_image_surface_get_height(image) != kCursorHeight) {
		clog << "Warning: Cursor image not correct size" << endl;
	}

	/* Draw the cursor. */
	cairo_set_source_surface(context, image, 0, 0);
	cairo_paint(context);
	cairo_destroy(context);
	cairo_surface_destroy(image);

	/* Make it visible. */
	SetVisible(true);
}

/** Destroy the cursor. */
Cursor::~Cursor() {
	delete m_window;
}

/** Set visibility of the cursor.
 * @param visible	New visibility state. */
void Cursor::SetVisible(bool visible) {
	m_window->SetVisible(visible);
}

/** Move the cursor relative to its current position.
 * @param dx		X delta.
 * @param dy		Y delta. */
void Cursor::MoveRelative(int32_t dx, int32_t dy) {
	int x = m_window->GetFrame().GetX() + dx;
	int y = m_window->GetFrame().GetY() + dy;

	/* Ensure that the location is within the screen. */
	if(x < -kCursorHotspotX) {
		x = -kCursorHotspotX;
	} else if(x >= (m_root->GetFrame().GetWidth() - kCursorHotspotX)) {
		x = (m_root->GetFrame().GetWidth() - kCursorHotspotX) - 1;
	}
	if(y < -kCursorHotspotY) {
		y = -kCursorHotspotY;
	} else if(y >= (m_root->GetFrame().GetHeight() - kCursorHotspotY)) {
		y = (m_root->GetFrame().GetHeight() - kCursorHotspotY) - 1;
	}

	/* Move the window to the new position. */
	dx = x - m_window->GetFrame().GetX();
	dy = y - m_window->GetFrame().GetY();
	m_window->MoveTo(Point(x, y));
}

/** Get the position of the cursor.
 * @return		Position of the cursor. */
Point Cursor::GetPosition() const {
	Point pos = m_window->GetAbsoluteFrame().GetTopLeft();
	return pos.Translated(kCursorHotspotX, kCursorHotspotY);
}
