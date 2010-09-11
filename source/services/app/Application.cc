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

#include <kernel/status.h>

#include <iostream>

#include "Application.h"
#include "Session.h"
#include "Surface.h"

using namespace std;
using namespace kiwi;

/** Construct an application object.
 * @param session	Session that the application is running under.
 * @param handle	Handle to the connection. */
Application::Application(Session *session, handle_t handle) :
	org::kiwi::AppServer::Session::ClientConnection(handle),
	m_session(session)
{
}

/** Destroy an application connection. */
Application::~Application() {
	/* Remove us from the session. */
	m_session->RemoveApplication(this);
}

/** Create a new session.
 * @param id		Where ID of new session will be placed.
 * @return		Status code describing result of the operation. */
status_t Application::CreateSession(session_id_t &id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Switch to a different session.
 * @param id		ID of session to switch to.
 * @return		Status code describing result of the operation. */
status_t Application::SwitchSession(session_id_t id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Create a new surface.
 *
 * Creates a new surface and returns the ID of an area referring to it. The
 * area ID is used to identify the surface on other surface calls. The created
 * surface will be in 32-bit (4 bytes per pixel) ARGB format.
 *
 * @param size		Size of the surface.
 * @param id		Where to store ID of surface's area.
 *
 * @return		Status code describing result of the operation.
 */
status_t Application::CreateSurface(org::kiwi::AppServer::Rect size, area_id_t &id) {
	try {
		Surface *surface = new Surface(size.width, size.height);
		m_session->AddSurface(surface);
		id = surface->GetID();
	} catch(OSError &e) {
		return e.GetCode();
	}

	return STATUS_SUCCESS;
}

/** Destroy a surface.
 * @param id		ID of the surface's area.
 * @return		Status code describing result of the operation. */
status_t Application::DestroySurface(area_id_t id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get the dimensions of a surface.
 * @param id		ID of the surface's area.
 * @param size		Where to store size of the surface.
 * @return		Status code describing result of the operation. */
status_t Application::GetSurfaceSize(area_id_t id, org::kiwi::AppServer::Rect &size) {
	Surface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	}

	size.width = surface->GetWidth();
	size.height = surface->GetHeight();
	return STATUS_SUCCESS;
}

/** Resize a surface.
 * @param id		ID of the surface's area.
 * @param size		New size for the surface.
 * @return		Status code describing result of the operation. */
status_t Application::ResizeSurface(area_id_t id, org::kiwi::AppServer::Rect size) {
	Surface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	}

	return surface->Resize(size.width, size.height);
}

/** Create a new window.
 * @param name		Title to give the window.
 * @param region	Location and dimensions of the window.
 * @param id		Where to store ID of window.
 * @return		Status code describing result of the operation. */
status_t Application::CreateWindow(const string &name, org::kiwi::AppServer::Region region,
                                   Window::ID &id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Destroy a window.
 * @param id		ID of window to destroy.
 * @return		Status code describing result of the operation. */
status_t Application::DestroyWindow(Window::ID id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get position and dimensions of a window.
 * @param id		ID of window.
 * @param region	Where to store position and dimensions.
 * @return		Status code describing result of the operation. */
status_t Application::GetWindowRegion(Window::ID id, org::kiwi::AppServer::Region &region) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get a window's surface.
 * @param id		ID of window.
 * @param sid		Where to store area ID for window's surface.
 * @return		Status code describing result of the operation. */
status_t Application::GetWindowSurface(Window::ID id, area_id_t &sid) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Change the title of a window.
 * @param id		ID of window.
 * @param name		New title for the window.
 * @return		Status code describing result of the operation. */
status_t Application::RenameWindow(Window::ID id, const string &name) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Move a window.
 * @param id		ID of window.
 * @param pos		New position for the window.
 * @return		Status code describing result of the operation. */
status_t Application::MoveWindow(Window::ID id, org::kiwi::AppServer::Point pos) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Resize a window.
 * @param id		ID of window.
 * @param size		New size for the window.
 * @return		Status code describing result of the operation. */
status_t Application::ResizeWindow(Window::ID id, org::kiwi::AppServer::Rect size) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Subscribe to events on a window.
 * @param id		ID of window.
 * @param events	Bitfield of events to subscribe to. This replaces the
 *			previous subscriptions.
 * @return		Status code describing result of the operation. */
status_t Application::SubscribeToWindow(Window::ID id, uint32_t mask) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Trigger an update of window on-screen.
 * @param id		ID of window.
 * @param region	Region to update. This is relative to the window, not
 *			to the entire screen. */
status_t Application::UpdateWindow(Window::ID id, org::kiwi::AppServer::Region region) {
	return STATUS_NOT_IMPLEMENTED;
}
