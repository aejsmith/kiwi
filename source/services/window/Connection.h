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
 * @brief		Window server connection class.
 */

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include <list>

#include "org.kiwi.WindowServer.h"
#include "ServerWindow.h"

class ServerSurface;
class Session;

/** Class representing a connection to the window server. */
class Connection : public org::kiwi::WindowServer::ClientConnection {
public:
	Connection(Session *session, handle_t handle);

	/** Get the session the connection is for.
	 * @return		Session that connection is for. */
	Session *GetSession() const { return m_session; }
private:
	status_t CreateSurface(org::kiwi::WindowServer::Size size, area_id_t &id);
	status_t DestroySurface(area_id_t id);
	status_t GetSurfaceSize(area_id_t id, org::kiwi::WindowServer::Size &size);
	status_t ResizeSurface(area_id_t id, org::kiwi::WindowServer::Size size);

	status_t CreateWindow(ServerWindow::ID &id);
	status_t DestroyWindow(ServerWindow::ID id);
	status_t CloseWindow(ServerWindow::ID id);
	status_t GetWindowTitle(ServerWindow::ID id, std::string &title);
	status_t SetWindowTitle(ServerWindow::ID id, const std::string &title);
	status_t GetWindowStyle(ServerWindow::ID id, uint32_t &style);
	status_t SetWindowStyle(ServerWindow::ID id, uint32_t style);
	status_t GetWindowLevel(ServerWindow::ID id, uint32_t &level);
	status_t SetWindowLevel(ServerWindow::ID id, uint32_t level);
	status_t GetWindowFrame(ServerWindow::ID id, org::kiwi::WindowServer::Rect &rect);
	status_t ResizeWindow(ServerWindow::ID id, org::kiwi::WindowServer::Size size);
	status_t MoveWindow(ServerWindow::ID id, org::kiwi::WindowServer::Point pos);
	status_t GetWindowState(ServerWindow::ID id, uint32_t &state);
	status_t SetWindowState(ServerWindow::ID id, uint32_t state);
	status_t ActivateWindow(ServerWindow::ID id);
	status_t GetWindowSurface(ServerWindow::ID id, area_id_t &sid);
	status_t UpdateWindow(ServerWindow::ID id, org::kiwi::WindowServer::Rect rect);

	void HandleHangup();

	Session *m_session;			/**< Session that the connection is on. */
	std::list<ServerWindow *> m_windows;	/**< Windows created by the connection. */
	std::list<ServerSurface *> m_surfaces;	/**< Surfaces created by the connection. */
};

#endif /* __CONNECTION_H */
