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

#ifndef __SERVERWINDOW_H
#define __SERVERWINDOW_H

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Graphics/Rect.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Object.h>

#include "org.kiwi.WindowServer.h"
#include "WindowList.h"

class Connection;
class Decoration;
class ServerSurface;
class Session;

/** Class implementing the server side of a window. */
class ServerWindow : public kiwi::Object, kiwi::Noncopyable {
public:
	/** Type of a window's ID. */
	typedef org::kiwi::WindowServer::WindowID ID;

	ServerWindow(Session *session, ID id, ServerWindow *parent, Connection *owner,
	             uint32_t style, uint32_t level, kiwi::Rect frame);
	~ServerWindow();

	void SetTitle(const std::string &title);
	void SetStyle(uint32_t style);
	void SetLevel(uint32_t level);
	kiwi::Rect GetAbsoluteFrame() const;
	kiwi::Rect GetAbsoluteTotalFrame() const;
	kiwi::Rect GetTotalFrame() const;
	void Resize(kiwi::Size size);
	void MoveTo(kiwi::Point pos);
	void SetState(uint32_t state);
	void SetActive(bool active);
	void Update();
	void Update(kiwi::Rect rect);
	ServerWindow *AtPosition(kiwi::Point pos);

	/** Get the session the window is under.
	 * @return		Session of the window. */
	Session *GetSession() const { return m_session; }

	/** Get the ID of the window.
	 * @return		ID of the window. */
	ID GetID() const { return m_id; }

	/** Get the owner of the window.
	 * @return		Owner of the window. */
	Connection *GetOwner() const { return m_owner; }

	/** Get the title of the window.
	 * @return		Title of the window. */
	const std::string &GetTitle() const { return m_title; }

	/** Get the window's style flags.
	 * @return		Window's style flags. */
	uint32_t GetStyle() const { return m_style; }

	/** Get the window's level.
	 * @return		Window's level. */
	uint32_t GetLevel() const { return m_level; }

	/** Get the window's frame.
	 * @return		Rectangle describing window's frame (its
	 *			size and position relative to the parent
	 *			window). */
	kiwi::Rect GetFrame() const { return m_frame; }

	/** Get the window's state flags.
	 * @return		Window's state flags. */
	uint32_t GetState() const { return m_state; }

	/** Set the visibility state of the window.
	 * @param visible	Whether to make the window visible. */
	void SetVisible(bool visible) {
		if(visible) {
			SetState(m_state | kiwi::BaseWindow::kVisibleState);
		} else {
			SetState(m_state & ~kiwi::BaseWindow::kVisibleState);
		}
	}

	/** Get the window surface.
	 * @return		Pointer to window's surface. */
	ServerSurface *GetSurface() const { return m_surface; }

	/** Get the window decoration.
	 * @return		Pointer to window decoration, or NULL if window
	 *			is undecorated. */
	Decoration *GetDecoration() const { return m_decoration; }

	/** Get the child list.
	 * @return		Reference to child list. */
	WindowList &GetChildren() { return m_children; }

	/** Check whether the window is visible.
	 * @return		Whether the window is visible. */
	bool IsVisible() const { return (m_state & kiwi::BaseWindow::kVisibleState); }

	/** Check whether the window or one of its children is active.
	 * @return		Whether the window is active. */
	bool IsActive() const { return (m_state & kiwi::BaseWindow::kActiveState); }
private:
	Session *m_session;		/**< Session that the window is on. */
	ID m_id;			/**< ID of the window. */
	ServerWindow *m_parent;		/**< Parent window. */
	Connection *m_owner;		/**< Owner of the window. */
	std::string m_title;		/**< Title of the window. */
	uint32_t m_style;		/**< Style flags for the window. */
	uint32_t m_level;		/**< Level of the window. */
	kiwi::Rect m_frame;		/**< Position/size of the window. */
	uint32_t m_state;		/**< State flags for the window. */
	ServerSurface *m_surface;	/**< Surface for the window. */
	Decoration *m_decoration;	/**< Decoration for the window. */
	WindowList m_children;		/**< Child windows. */
};

#endif /* __WINDOW_H */
