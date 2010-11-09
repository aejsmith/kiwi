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
#include "Connection.h"
#include "Decoration.h"
#include "ServerSurface.h"
#include "ServerWindow.h"
#include "Session.h"

using namespace kiwi;
using namespace std;

/** Create a window.
 * @param session	Session that the window is on.
 * @param id		ID for the window. If negative, the window will not be
 *			published in the session.
 * @param parent	Parent of the window.
 * @param owner		Connection that owns the window.
 * @param style		Style flags for the window.
 * @param level		Level for the window.
 * @param frame		Initial frame for the window. */
ServerWindow::ServerWindow(Session *session, ID id, ServerWindow *parent, Connection *owner,
	                   uint32_t style, uint32_t level, Rect frame) :
	m_session(session), m_id(id), m_parent(parent), m_owner(owner),
	m_style(style), m_level(level), m_frame(frame), m_state(0),
	m_surface(0), m_decoration(0)
{
	/* Create a new surface and publish it in the session. FIXME: Need to
	 * stop a DestroySurface call on this surface from working: reference
	 * count, 1 for link to session, 1 for link to window. */
	m_surface = new ServerSurface(owner, m_frame.GetSize());
	if(m_id >= 0) {
		m_session->AddSurface(m_surface);
	}

	/* Create a decoration if necessary. */
	if(m_style & BaseWindow::kBorderMask) {
		m_decoration = new Decoration(this);
	}

	/* If no parent (the root window), set the visible and active flags. */
	if(!parent) {
		m_state = BaseWindow::kVisibleState | BaseWindow::kActiveState;
	}
}

/** Destroy the window. */
ServerWindow::~ServerWindow() {
	m_owner = 0;

	SetVisible(false);
	if(m_id >= 0) {
		if(m_surface) {
			m_session->RemoveSurface(m_surface);
		}
		m_session->RemoveWindow(this);
	}
	if(m_decoration) {
		delete m_decoration;
	}
	if(m_surface) {
		delete m_surface;
	}
}

/** Set title of the window.
 * @param title		New title. */
void ServerWindow::SetTitle(const std::string &title) {
	m_title = title;
	if(m_decoration) {
		m_decoration->Update();
		Update();
	}

	/* Send a title change event. */
	if(m_owner) {
		m_owner->OnWindowTitleChange(m_id);
	}
}

/** Set the style of the window.
 * @param style		New style. */
void ServerWindow::SetStyle(uint32_t style) {
	bool update = false;

	m_style = style;

	/* Create or destroy the decoration of the window if required. */
	if(m_style & BaseWindow::kBorderMask) {
		if(!m_decoration) {
			m_decoration = new Decoration(this);
			update = true;
		}
	} else {
		if(m_decoration) {
			delete m_decoration;
			m_decoration = 0;
			update = true;
		}
	}

	/* Redraw if the decoration was changed. */
	if(update) {
		Update();
	}
}

/** Set the level of the window.
 * @param level		New level. */
void ServerWindow::SetLevel(uint32_t level) {
	if(m_parent && IsVisible()) {
		m_parent->m_children.Remove(this);
	}

	m_level = level;

	if(m_parent && IsVisible()) {
		m_parent->m_children.Insert(this);
		Update();
	}
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, not including area covered
 *			by decoration. To get including decoration, use
 *			GetAbsoluteTotalFrame(). */
Rect ServerWindow::GetAbsoluteFrame() const {
	Rect ret = m_frame;
	if(m_parent) {
		Rect parent = m_parent->GetAbsoluteFrame();
		ret.Translate(parent.GetX(), parent.GetY());
	}
	return ret;
}

/** Get the absolute area of the window on the screen.
 * @return		Absolute area of window, including area covered by
 *			decoration. */
Rect ServerWindow::GetAbsoluteTotalFrame() const {
	Rect ret = GetTotalFrame();
	if(m_parent) {
		Rect parent = m_parent->GetAbsoluteFrame();
		ret.Adjust(parent.GetX(), parent.GetY(), parent.GetX(), parent.GetY());
	}
	return ret;
}

/** Get the total area including decoration.
 * @return		Total area including decoration, relative to the
 *			window's parent. */
Rect ServerWindow::GetTotalFrame() const {
	if(m_decoration) {
		/* The decoration position is relative to the window position. */
		return Rect(m_frame.GetX() + m_decoration->GetFrame().GetX(),
		            m_frame.GetY() + m_decoration->GetFrame().GetY(),
		            m_decoration->GetFrame().GetWidth(),
		            m_decoration->GetFrame().GetHeight());
	} else {
		return m_frame;
	}
}

/** Change the size of the window.
 * @param size		New window size. */
void ServerWindow::Resize(Size size) {
	/* Create a region to work out the update region. */
	Region update(GetAbsoluteTotalFrame());

	/* Save the previous size. */
	org::kiwi::WindowServer::Size prev = { m_frame.GetWidth(), m_frame.GetHeight() };

	/* Set the new size and update the decoration (if any). */
	m_frame.Resize(size);
	if(m_decoration) {
		m_decoration->Update();
	}

	/* If there is an owner, send it a resize event and leave it to them to
	 * resize the window's surface. This is so that we do not change the
	 * size of the surface when the client doesn't expect it. */
	if(m_owner) {
		org::kiwi::WindowServer::Size csize = { m_frame.GetWidth(), m_frame.GetHeight() };
		m_owner->OnWindowResize(m_id, csize, prev);
	} else {
		m_surface->Resize(size);
	}

	/* Add the new frame to the update region and update. */
	if(IsVisible() && m_session->IsActive()) {
		update.Union(GetAbsoluteTotalFrame());
		m_session->GetCompositor()->Redraw(update);
	}
}

/** Move the window.
 * @param pos		New position for window. */
void ServerWindow::MoveTo(Point pos) {
	if(m_level == BaseWindow::kRootLevel) {
		return;
	}

	/* Create a region to work out the update region. */
	Region update(GetAbsoluteTotalFrame());

	/* Move ourself. */
	m_frame.MoveTo(pos);

	/* Update the screen. */
	if(IsVisible() && m_session->IsActive()) {
		update.Union(GetAbsoluteTotalFrame());
		m_session->GetCompositor()->Redraw(update);
	}
}

/** Set the state of the window.
 * @param state		New state of the window. */
void ServerWindow::SetState(uint32_t state) {
	/* Cannot change kActiveState through this. */
	state &= ~BaseWindow::kActiveState;
	state |= (m_state & BaseWindow::kActiveState);

	/* Get the set of changed flags and store the new ones. */
	uint32_t prev = m_state;
	uint32_t changed = prev ^ state;
	m_state = state;

	if(changed) {
		bool update = false;

		if(changed & BaseWindow::kVisibleState) {
			if(m_parent) {
				if(m_state & BaseWindow::kVisibleState) {
					m_parent->GetChildren().Insert(this);
				} else {
					m_parent->GetChildren().Remove(this);
				}
			}

			update = true;
		}

		/* Redraw if required. */
		if(update && m_session->IsActive()) {
			m_session->GetCompositor()->Redraw(GetAbsoluteTotalFrame());
		}

		/* Send a state change event to the owner. */
		if(m_owner) {
			m_owner->OnWindowStateChange(m_id, m_state, prev);
		}
	}
}

/** Set whether the window is active.
 * @param active	Whether to make the window active. */
void ServerWindow::SetActive(bool active) {
	/* Cursors should not be made active. */
	assert(m_level != BaseWindow::kCursorLevel);

	if(IsActive() != active) {
		bool update = false;

		/* Set the state flag. */
		uint32_t prev = m_state;
		if(active) {
			m_state |= BaseWindow::kActiveState;
		} else {
			m_state &= ~BaseWindow::kActiveState;
		}

		/* Update the decoration. We always need a redraw if the
		 * decoration changes. */
		if(m_decoration) {
			m_decoration->Update();
			update = true;
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
		if(active && m_parent) {
			if(m_parent->GetChildren().MoveToFront(this)) {
				update = true;
			}
		}

		/* Send a state change event to the owner. */
		if(m_owner) {
			m_owner->OnWindowStateChange(m_id, m_state, prev);
		}

		/* Redraw if required. */
		if(update) {
			Update();
		}
	}
}

/** Update the entire window. */
void ServerWindow::Update() {
	if(IsVisible() && m_session->IsActive()) {
		m_session->GetCompositor()->Redraw(GetAbsoluteTotalFrame());
	}
}

/** Update an area on the window.
 * @param rect		Rectangle area to update. */
void ServerWindow::Update(kiwi::Rect rect) {
	if(IsVisible() && m_session->IsActive()) {
		/* The provided rectangle is relative to our position. Adjust
		 * it to be an on-screen position. */
		Rect abs = GetAbsoluteFrame();
		rect.Translate(abs.GetX(), abs.GetY());

		/* Redraw the area on screen. */
		m_session->GetCompositor()->Redraw(abs.Intersected(rect));
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
ServerWindow *ServerWindow::AtPosition(kiwi::Point pos) {
	/* Can't do this for cursor windows. */
	if(m_level == BaseWindow::kCursorLevel) {
		return 0;
	}

	/* Check if it is within children. */
	for(auto it = m_children.ReverseBegin(); it != m_children.ReverseEnd(); ++it) {
		ServerWindow *ret = (*it)->AtPosition(pos);
		if(ret) {
			return ret;
		}
	}

	/* Not within children, is it within ourself? */
	if(GetAbsoluteTotalFrame().Contains(pos)) {
		return this;
	}

	return 0;
}

/** Convert a point to be relative to the window.
 * @param pos		Absolute position to convert.
 * @return		Position relative to the window. */
kiwi::Point ServerWindow::RelativePoint(kiwi::Point pos) {
	Rect frame = GetAbsoluteFrame();
	return pos.Translated(-frame.GetX(), -frame.GetY());
}

/** Handle a mouse move event on the window.
 * @param event		Event object containing event details. */
void ServerWindow::MouseMove(const MouseEvent &event) {
	if(event.GetPosition().GetX() < 0 || event.GetPosition().GetY() < 0) {
		assert(m_decoration);
		m_decoration->MouseMove(event);
	} else if(m_owner) {
		org::kiwi::WindowServer::Point pos = {
			event.GetPosition().GetX(), event.GetPosition().GetY()
		};
		m_owner->OnMouseMove(m_id, event.GetTime(), event.GetModifiers(),
		                     pos, event.GetButtons());
	}
}

/** Handle a mouse press event on the window.
 * @param event		Event object containing event details. */
void ServerWindow::MousePress(const MouseEvent &event) {
	if(event.GetPosition().GetX() < 0 || event.GetPosition().GetY() < 0) {
		assert(m_decoration);
		m_decoration->MousePress(event);
	} else if(m_owner) {
		org::kiwi::WindowServer::Point pos = {
			event.GetPosition().GetX(), event.GetPosition().GetY()
		};
		m_owner->OnMousePress(m_id, event.GetTime(), event.GetModifiers(),
		                      pos, event.GetButtons());
	}
}

/** Handle a mouse release event on the window.
 * @param event		Event object containing event details. */
void ServerWindow::MouseRelease(const MouseEvent &event) {
	if(event.GetPosition().GetX() < 0 || event.GetPosition().GetY() < 0) {
		assert(m_decoration);
		m_decoration->MouseRelease(event);
	} else if(m_owner) {
		org::kiwi::WindowServer::Point pos = {
			event.GetPosition().GetX(), event.GetPosition().GetY()
		};
		m_owner->OnMouseRelease(m_id, event.GetTime(), event.GetModifiers(),
		                        pos, event.GetButtons());
	}
}

/** Handle a key press event on the window.
 * @param event		Event object containing event details. */
void ServerWindow::KeyPress(const KeyEvent &event) {
	if(m_owner) {
		m_owner->OnKeyPress(m_id, event.GetTime(), event.GetModifiers(),
		                    event.GetKey(), event.GetText());
	}
}

/** Handle a key press event on the window.
 * @param event		Event object containing event details. */
void ServerWindow::KeyRelease(const KeyEvent &event) {
	if(m_owner) {
		m_owner->OnKeyRelease(m_id, event.GetTime(), event.GetModifiers(),
		                      event.GetKey(), event.GetText());
	}
}
