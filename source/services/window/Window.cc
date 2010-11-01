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

#include <cassert>

#include "Compositor.h"
#include "Decoration.h"
#include "Session.h"
#include "Surface.h"
#include "Window.h"

using namespace kiwi;
using namespace std;

/** Create a window.
 * @param session	Session that the window is on.
 * @param id		ID for the window. If negative, the window will not be
 *			published in the session.
 * @param rect		Rectangle describing window area. */
Window::Window(Session *session, ID id, Window *parent, const kiwi::Rect &rect, window_type type) :
	m_session(session), m_id(id), m_parent(parent), m_rect(rect),
	m_type(type), m_decoration(0), m_visible(false), m_active(false)
{
	/* Create a new surface and publish it in the session. FIXME: Somehow
	 * need to stop a DestroySurface call on this surface from working. */
	m_surface = new Surface(rect.GetWidth(), rect.GetHeight());
	if(m_id >= 0) {
		m_session->AddSurface(m_surface);
	}

	/* Create the window decoration if the type requires one. */
	switch(m_type) {
	case WINDOW_TYPE_NORMAL:
	case WINDOW_TYPE_ALERT:
		m_decoration = new Decoration(this);
		break;
	default:
		break;
	};

	/* If there is no parent (the root window) make us visible and active. */
	if(!m_parent) {
		m_visible = true;
		m_active = true;
	}
}

/** Destroy the window. */
Window::~Window() {
	SetVisible(false);
	if(m_id >= 0) {
		m_session->RemoveSurface(m_surface);
		m_session->RemoveWindow(this);
	}
	delete m_surface;
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, not including area covered
 *			by decoration. To get including decoration, use
 *			GetAbsoluteTotalRect(). */
Rect Window::GetAbsoluteRect() const {
	Rect ret = m_rect;
	if(m_parent) {
		Rect parent = m_parent->GetAbsoluteRect();
		ret.Adjust(parent.GetX(), parent.GetY(), parent.GetX(), parent.GetY());
	}
	return ret;
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, including area covered by
 *			decoration. */
Rect Window::GetAbsoluteTotalRect() const {
	Rect ret = GetTotalRect();
	if(m_parent) {
		Rect parent = m_parent->GetAbsoluteRect();
		ret.Adjust(parent.GetX(), parent.GetY(), parent.GetX(), parent.GetY());
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
		m_decoration->Update();
		m_session->GetCompositor()->Redraw(GetAbsoluteTotalRect());
	}
}

/** Set visibility of the window.
 * @note		Use Session::ActivateWindow() or Session::HideWindow()
 *			instead of this!
 * @param visible	New visibility state. */
void Window::SetVisible(bool visible) {
	if(m_visible != visible) {
		m_visible = visible;
		if(m_parent) {
			if(visible) {
				m_parent->GetChildren().Insert(this);
			} else {
				m_parent->GetChildren().Remove(this);
			}
		}
		m_session->GetCompositor()->Redraw(GetAbsoluteTotalRect());
	}
}

/** Set whether the window is active.
 * @note		Use Session::ActivateWindow() instead of this!
 * @param active	New active state. */
void Window::SetActive(bool active) {
	/* Cursors should not be made active. */
	assert(!active || m_type != WINDOW_TYPE_CURSOR);

	if(m_active != active) {
		bool redraw = false;

		m_active = active;

		/* Update the decoration. We always need a redraw if the
		 * decoration changes. TODO: Redraw only the decoration area. */
		if(m_decoration) {
			m_decoration->Update();
			redraw = true;
		}

		/* We must also change the active state on the parent window.
		 * This is to ensure that, for example, when a child menu
		 * window is active, the decoration on the window that the
		 * menu is for appears as active. */
		if(m_parent) {
			m_parent->SetActive(active);
		}

		/* If we are now active, ensure that we are brought above all
		 * windows in our parent. If the window list position changed,
		 * perform a redraw. */
		if(m_active && m_parent) {
			if(m_parent->GetChildren().MoveToFront(this)) {
				redraw = true;
			}
		}

		if(redraw && m_visible) {
			m_session->GetCompositor()->Redraw(GetAbsoluteTotalRect());
		}
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
	m_session->GetCompositor()->Redraw(abs.Intersected(rect));
}

/** Move the window.
 * @param pos		New position for window. */
void Window::MoveTo(const kiwi::Point &pos) {
	if(m_type == WINDOW_TYPE_ROOT) {
		return;
	}

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

/** Get window at position.
 *
 * If the specified point is within a child window, returns that window. The
 * child list is searched end to first, meaning the front-most child containing
 * the point will be returned. If the point isn't within a child, and it is
 * within the window itself, the window itself will be returned. Otherwise,
 * NULL will be returned.
 *
 * @param pos		Absolute screen position.
 *
 * @return		Child at position, window itself, or NULL if point
 *			outside window and children.
 */
Window *Window::AtPosition(const kiwi::Point &pos) {
	/* Can't do this for cursor windows. */
	if(m_type == WINDOW_TYPE_CURSOR) {
		return 0;
	}

	/* Check if it is within children. */
	for(auto it = m_children.RBegin(); it != m_children.REnd(); ++it) {
		Window *ret = (*it)->AtPosition(pos);
		if(ret) {
			return ret;
		}
	}

	/* Not within children, is it within ourself? */
	if(GetAbsoluteTotalRect().Contains(pos)) {
		return this;
	}

	return 0;
}
