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
 * @brief		Window server connection class.
 */

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "org.kiwi.WindowServer.h"
#include "Window.h"

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
	status_t ResizeSurface(area_id_t id, org::kiwi::WindowServer::Size size);
	status_t GetSurfaceSize(area_id_t id, org::kiwi::WindowServer::Size &size);

	status_t CreateWindow(uint32_t style, uint32_t level, Window::ID &id);
	status_t DestroyWindow(Window::ID id);
	status_t CloseWindow(Window::ID id);
	status_t GetWindowTitle(Window::ID id, std::string &title);
	status_t SetWindowTitle(Window::ID id, const std::string &title);
	status_t GetWindowStyle(Window::ID id, uint32_t &style);
	status_t SetWindowStyle(Window::ID id, uint32_t style);
	status_t GetWindowLevel(Window::ID id, uint32_t &level);
	status_t SetWindowLevel(Window::ID id, uint32_t level);
	status_t GetWindowFrame(Window::ID id, org::kiwi::WindowServer::Rect &rect);
	status_t ResizeWindow(Window::ID id, org::kiwi::WindowServer::Size size);
	status_t MoveWindow(Window::ID id, org::kiwi::WindowServer::Point pos);
	status_t GetWindowState(Window::ID id, uint32_t &state);
	status_t SetWindowState(Window::ID id, uint32_t state);
	status_t GetWindowSurface(Window::ID id, area_id_t &sid);
	status_t UpdateWindow(Window::ID id, org::kiwi::WindowServer::Rect rect);

	void HandleHangup();

	Session *m_session;		/**< Session that the connection is on. */
};

#endif /* __CONNECTION_H */
