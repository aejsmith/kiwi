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
 * @brief		UI session class.
 */

#ifndef __SESSION_H
#define __SESSION_H

#include <kiwi/Object.h>
#include <kiwi/Rect.h>

#include <kernel/types.h>

#include <list>
#include <map>

#include "Window.h"

class Connection;
class Surface;
class Compositor;
class WindowServer;

/** Class containing information of a UI session. */
class Session : public kiwi::Object, public kiwi::internal::Noncopyable {
	/** Type of the connection list. */
	typedef std::list<Connection *> ConnectionList;

	/** Type of the surface map. */
	typedef std::map<area_id_t, Surface *> SurfaceMap;

	/** Type of the window map. */
	typedef std::map<Window::ID, Window *> WindowMap;
public:
	Session(WindowServer *server, session_id_t id);

	void HandleConnection(handle_t handle);
	void RemoveConnection(Connection *conn);

	void AddSurface(Surface *surface);
	void RemoveSurface(Surface *surface);
	Surface *FindSurface(area_id_t id);

	Window *CreateWindow(kiwi::Rect &rect);
	void RemoveWindow(Window *window);
	Window *FindWindow(Window::ID id);
	void ActivateWindow(Window *window);
	void HideWindow(Window *window);

	void Activate();
	void Deactivate();

	/** Get the ID of the session.
	 * @return		ID of the session. */
	session_id_t GetID() const { return m_id; }

	/** Get the root window.
	 * @return		Root window for the session. */
	Window *GetRoot() const { return m_root; }

	/** Get the session's compositor.
	 * @return		Pointer to session's compositor. */
	Compositor *GetCompositor() const { return m_compositor; }

	/** Get the active window.
	 * @return		Pointer to active window. */
	Window *GetActiveWindow() const { return m_active_window; }
private:
	void Release();

	ConnectionList m_connections;	/**< Connections on the session. */
	SurfaceMap m_surfaces;		/**< Map of surfaces in the session. */
	WindowMap m_windows;		/**< Map of windows in the session. */
	WindowServer *m_server;		/**< Server that the session is on. */
	session_id_t m_id;		/**< ID of the session. */
	bool m_active;			/**< Whether the session is active. */
	int m_refcount;			/**< Reference count. */
	Window *m_root;			/**< Root window. */
	Compositor *m_compositor;	/**< Compositor. */
	Window::ID m_next_window_id;	/**< Next window ID. */
	Window *m_active_window;	/**< Active window. */
};

#endif /* __SESSION_H */
