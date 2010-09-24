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

#ifndef __WINDOW_H
#define __WINDOW_H

#include <kiwi/Object.h>
#include <kiwi/Rect.h>

#include "org.kiwi.WindowServer.h"
#include "WindowList.h"

class Decoration;
class Session;
class Surface;

// FIXME: Move to public location.
enum window_type {
	WINDOW_TYPE_ROOT,	// do not allow from clients.
	WINDOW_TYPE_NORMAL,	// allow for top-levels only.
	WINDOW_TYPE_UNBORDERED,	// same. but no border.
	WINDOW_TYPE_ALERT,	// same.
	WINDOW_TYPE_PANEL,	// same. fixed above all windows in the WindowList its in.
	WINDOW_TYPE_CHILD,	// allow for children only.
	WINDOW_TYPE_POPUP,	// same.
};

/** Class implementing a window. */
class Window : public kiwi::Object, public kiwi::internal::Noncopyable {
public:
	/** Type of a window's ID. */
	typedef org::kiwi::WindowServer::WindowID ID;

	Window(Session *session, ID id, Window *parent, const kiwi::Rect &rect, window_type type);
	~Window();

	kiwi::Rect GetAbsoluteRect() const;
	kiwi::Rect GetAbsoluteTotalRect() const;
	kiwi::Rect GetTotalRect() const;

	void SetTitle(const std::string &title);
	void SetVisible(bool visible);
	void Update(kiwi::Rect rect);
	void MoveTo(const kiwi::Point &pos);

	/** Get the ID of the window.
	 * @return		ID of the window. */
	ID GetID() const { return m_id; }

	/** Get the window rectangle.
	 * @return		Rectangle describing window's position/size,
	 *			relative to the parent window. */
	kiwi::Rect GetRect() const { return m_rect; }

	/** Get the type of the window.
	 * @return		Type of the window. */
	window_type GetType() const { return m_type; }

	/** Get the title of the window.
	 * @return		Title of the window. */
	const std::string &GetTitle() const { return m_title; }

	/** Get the window surface.
	 * @return		Pointer to window's surface. */
	Surface *GetSurface() const { return m_surface; }

	/** Get the window decoration.
	 * @return		Pointer to window decoration, or NULL if window
	 *			is undecorated. */
	Decoration *GetDecoration() const { return m_decoration; }

	/** Get the child list.
	 * @return		Reference to child list. */
	WindowList &GetChildren() { return m_children; }

	/** Check whether the window is visible.
	 * @return		Whether the window is visible. */
	bool IsVisible() const { return m_visible; }
private:
	Session *m_session;		/**< Session that the window is on. */
	ID m_id;			/**< ID of the window. */
	Window *m_parent;		/**< Parent window. */
	kiwi::Rect m_rect;		/**< Position/size of the window. */
	window_type m_type;		/**< Type of the window. */
	std::string m_title;		/**< Title of the window. */
	Surface *m_surface;		/**< Surface for the window. */
	Decoration *m_decoration;	/**< Decoration for the window. */
	WindowList m_children;		/**< Child windows. */
	bool m_visible : 1;		/**< Whether the window is visible. */
};

#endif /* __WINDOW_H */
