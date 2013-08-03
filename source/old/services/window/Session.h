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
 * @brief		UI session class.
 */

#ifndef __SESSION_H
#define __SESSION_H

#include <kiwi/Graphics/InputEvent.h>
#include <kiwi/Graphics/Rect.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Object.h>

#include <kernel/types.h>

#include <list>
#include <map>

#include "MouseReceiver.h"
#include "ServerWindow.h"

class Connection;
class Cursor;
class ServerSurface;
class Compositor;
class WindowServer;

/** Class containing information of a UI session. */
class Session : public kiwi::Object, kiwi::Noncopyable {
	/** Type of the connection list. */
	typedef std::list<Connection *> ConnectionList;

	/** Type of the surface map. */
	typedef std::map<area_id_t, ServerSurface *> SurfaceMap;

	/** Type of the window map. */
	typedef std::map<ServerWindow::ID, ServerWindow *> WindowMap;
public:
	Session(WindowServer *server, session_id_t id);
	~Session();

	void HandleConnection(handle_t handle);
	void RemoveConnection(Connection *conn);

	void AddSurface(ServerSurface *surface);
	void RemoveSurface(ServerSurface *surface);
	ServerSurface *FindSurface(area_id_t id);

	ServerWindow *CreateWindow(Connection *owner);
	void RemoveWindow(ServerWindow *window);
	ServerWindow *FindWindow(ServerWindow::ID id);
	ServerWindow *WindowAtCursor();
	void ActivateWindow(ServerWindow *window);

	void Activate();
	void Deactivate();

	void MouseMoved(useconds_t time, int dx, int dy, uint32_t modifiers, uint32_t buttons);
	void MousePressed(useconds_t time, uint32_t modifiers, uint32_t buttons);
	void MouseReleased(useconds_t time, uint32_t modifiers, uint32_t buttons);
	void KeyPressed(const kiwi::KeyEvent &event);
	void KeyReleased(const kiwi::KeyEvent &event);

	void GrabMouse(MouseReceiver *object);
	void ReleaseMouse();

	/** Get the ID of the session.
	 * @return		ID of the session. */
	session_id_t GetID() const { return m_id; }

	/** Check whether the session is active.
	 * @return		Whether the session is the active session. */
	bool IsActive() const { return m_active; }

	/** Get the root window.
	 * @return		Root window for the session. */
	ServerWindow *GetRoot() const { return m_root; }

	/** Get the cursor.
	 * @return		Cursor for the session. */
	Cursor *GetCursor() const { return m_cursor; }

	/** Get the session's compositor.
	 * @return		Pointer to session's compositor. */
	Compositor *GetCompositor() const { return m_compositor; }

	/** Get the active window.
	 * @return		Pointer to active window. */
	ServerWindow *GetActiveWindow() const { return m_active_window; }
private:
	MouseReceiver *MouseEventTarget(kiwi::Point &pos, bool activate = false);
	void Release();

	ConnectionList m_connections;	/**< Connections on the session. */
	SurfaceMap m_surfaces;		/**< Map of surfaces in the session. */
	WindowMap m_windows;		/**< Map of windows in the session. */
	WindowServer *m_server;		/**< Server that the session is on. */
	session_id_t m_id;		/**< ID of the session. */
	bool m_active;			/**< Whether the session is active. */
	int m_refcount;			/**< Reference count. */
	ServerWindow *m_root;		/**< Root window. */
	Cursor *m_cursor;		/**< Cursor. */
	Compositor *m_compositor;	/**< Compositor. */
	ServerWindow::ID m_next_wid;	/**< Next window ID. */
	ServerWindow *m_active_window;	/**< Active window. */
	MouseReceiver *m_mouse_grabber;	/**< Object that has grabbed the mouse. */
};

#endif /* __SESSION_H */