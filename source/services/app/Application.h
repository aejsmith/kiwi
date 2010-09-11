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
 * @brief		Application class.
 */

#ifndef __APPLICATION_H
#define __APPLICATION_H

#include "org.kiwi.AppServer.h"
#include "Session.h"
#include "Window.h"

/** Class representing an application's connection to the server. */
class Application : public org::kiwi::AppServer::Session::ClientConnection {
public:
	Application(Session *session, handle_t handle);
	~Application();
private:
	status_t CreateSession(session_id_t &id);
	status_t SwitchSession(session_id_t id);
	status_t CreateSurface(org::kiwi::AppServer::Rect size, area_id_t &id);
	status_t DestroySurface(area_id_t id);
	status_t GetSurfaceSize(area_id_t id, org::kiwi::AppServer::Rect &size);
	status_t ResizeSurface(area_id_t id, org::kiwi::AppServer::Rect size);
	status_t CreateWindow(const std::string &name, org::kiwi::AppServer::Region region, Window::ID &id);
	status_t DestroyWindow(Window::ID id);
	status_t GetWindowRegion(Window::ID id, org::kiwi::AppServer::Region &region);
	status_t GetWindowSurface(Window::ID id, area_id_t &sid);
	status_t RenameWindow(Window::ID id, const std::string &name);
	status_t MoveWindow(Window::ID id, org::kiwi::AppServer::Point pos);
	status_t ResizeWindow(Window::ID id, org::kiwi::AppServer::Rect size);
	status_t SubscribeToWindow(Window::ID id, uint32_t events);
	status_t UpdateWindow(Window::ID id, org::kiwi::AppServer::Region region);

	Session *m_session;		/**< Session that the application is running under. */
};

#endif /* __APPLICATION_H */
