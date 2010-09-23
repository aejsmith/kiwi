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
 * @brief		Window class.
 */

#include "Compositor.h"
#include "Decoration.h"
#include "Session.h"
#include "Surface.h"
#include "Window.h"

using namespace kiwi;

/** Create a window.
 * @param session	Session that the window is on.
 * @param id		ID for the window.
 * @param rect		Rectangle describing window area. */
Window::Window(Session *session, ID id, Window *parent, const kiwi::Rect &rect, window_type type) :
	m_session(session), m_id(id), m_parent(parent), m_rect(rect),
	m_type(type), m_decoration(0), m_visible(false)
{
	/* Create a new surface and publish it in the session. FIXME: Somehow
	 * need to stop a DestroySurface call on this surface from working. */
	m_surface = new Surface(rect.GetWidth(), rect.GetHeight());
	m_session->AddSurface(m_surface);

	/* Create the window decoration if the type requires one. */
	switch(m_type) {
	case WINDOW_TYPE_NORMAL:
	case WINDOW_TYPE_ALERT:
		m_decoration = new Decoration(this);
		break;
	default:
		break;
	};

	/* If there is no parent (the root window) make it visible. */
	if(!m_parent) {
		m_visible = true;
	}
}

/** Destroy the window. */
Window::~Window() {
	/* Remove the window from the window manager. */
	SetVisible(false);
	m_session->RemoveSurface(m_surface);
	delete m_surface;
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, not including area covered
 *			by decoration. To get including decoration, use
 *			GetAbsoluteTotalRect(). */
Rect Window::GetAbsoluteRect() const {
	Rect ret = m_rect;
	if(m_parent) {
		ret.Adjust(m_parent->GetRect().GetX(), m_parent->GetRect().GetY(),
		           m_parent->GetRect().GetX(), m_parent->GetRect().GetY());
	}
	return ret;
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, including area covered by
 *			decoration. */
Rect Window::GetAbsoluteTotalRect() const {
	Rect ret = GetTotalRect();
	if(m_parent) {
		ret.Adjust(m_parent->GetRect().GetX(), m_parent->GetRect().GetY(),
		           m_parent->GetRect().GetX(), m_parent->GetRect().GetY());
	}
	return ret;
}

/** Get the total area including decoration.
 * @return		Total area including decoration, relative to the
 *			window's parent. */
Rect Window::GetTotalRect() const {
	if(m_decoration) {
		/* The decoration position is relative to the window position. */
		return Rect(m_rect.GetX() + m_decoration->GetRect().GetX(),
		            m_rect.GetY() + m_decoration->GetRect().GetY(),
		            m_decoration->GetRect().GetWidth(),
		            m_decoration->GetRect().GetHeight());
	} else {
		return m_rect;
	}
}

/** Set title of the window.
 * @param title		New title. */
void Window::SetTitle(const std::string &title) {
	m_title = title;
	if(m_decoration) {
		m_decoration->Redraw();
	}
}

/** Set visibility of the window.
 * @param visible	New visibility state. */
void Window::SetVisible(bool visible) {
	if(m_visible != visible) {
		m_visible = visible;
		if(m_parent) {
			if(visible) {
				m_parent->GetChildren().AddWindow(this);
			} else {
				m_parent->GetChildren().RemoveWindow(this);
			}
		}
		m_session->GetCompositor()->Redraw(GetAbsoluteTotalRect());
	}
}

/** Update an area on the window.
 * @param rect		Rectangle area to update. */
void Window::Update(kiwi::Rect rect) {
	/* The provided rectangle is relative to our position. Adjust it to be
	 * an on-screen position. */
	Rect abs = GetAbsoluteRect();
	rect.Adjust(abs.GetX(), abs.GetY(), abs.GetX(), abs.GetY());

	/* Redraw the area on screen. */
	m_session->GetCompositor()->Redraw(abs.Intersect(rect));
}

/** Move the window.
 * @param pos		New position for window. */
void Window::MoveTo(const kiwi::Point &pos) {
	/* Create a region to work out the update region. */
	Region update(GetAbsoluteTotalRect());

	/* Move ourself. */
	m_rect.MoveTo(pos);

	/* Update the screen. */
	if(m_visible) {
		update.Union(GetAbsoluteTotalRect());
		m_session->GetCompositor()->Redraw(update);
	}
}
