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

#include "Connection.h"
#include "Session.h"
#include "Surface.h"
#include "WindowServer.h"

using namespace kiwi;

/** Create a connection object.
 * @param session	Session the connection is from.
 * @param handle	Handle to the connection. */
Connection::Connection(Session *session, handle_t handle) :
	org::kiwi::WindowServer::ClientConnection(handle),
	m_session(session)
{
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
status_t Connection::CreateSurface(org::kiwi::WindowServer::Size size, area_id_t &id) {
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
status_t Connection::DestroySurface(area_id_t id) {
	Surface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	}

	m_session->RemoveSurface(surface);
	delete surface;
	return STATUS_SUCCESS;
}

/** Resize a surface.
 * @param id		ID of the surface's area.
 * @param size		New size for the surface.
 * @return		Status code describing result of the operation. */
status_t Connection::ResizeSurface(area_id_t id, org::kiwi::WindowServer::Size size) {
	Surface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	}

	return surface->Resize(size.width, size.height);
}

/** Get the size of a surface.
 * @param id		ID of the surface's area.
 * @param size		Where to store surface size.
 * @return		Status code describing result of the operation. */
status_t Connection::GetSurfaceSize(area_id_t id, org::kiwi::WindowServer::Size &size) {
	Surface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	}

	size.width = surface->GetWidth();
	size.height = surface->GetHeight();
	return STATUS_SUCCESS;
}

/** Create a new window.
 * @param rect		Rectangle area for the window.
 * @param id		Where to store ID of window.
 * @return		Status code describing result of the operation. */
status_t Connection::CreateWindow(org::kiwi::WindowServer::Rect rect, Window::ID &id) {
	try {
		Rect _rect(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);
		Window *window = m_session->CreateWindow(_rect);
		id = window->GetID();
	} catch(OSError &e) {
		return e.GetCode();
	}

	return STATUS_SUCCESS;
}

status_t Connection::DestroyWindow(Window::ID id) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t Connection::SubscribeWindow(Window::ID id, uint32_t events) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Show a window.
 * @param id		ID of window to show.
 * @return		Status code describing result of the operation. */
status_t Connection::ShowWindow(Window::ID id) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	m_session->ActivateWindow(window);
	return STATUS_SUCCESS;
}

/** Hide a window.
 * @param id		ID of window to hide.
 * @return		Status code describing result of the operation. */
status_t Connection::HideWindow(Window::ID id) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	m_session->HideWindow(window);
	return STATUS_SUCCESS;
}

status_t Connection::CloseWindow(Window::ID id) {
	return STATUS_NOT_IMPLEMENTED;
}

status_t Connection::ResizeWindow(Window::ID id, org::kiwi::WindowServer::Size size) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Move a window.
 * @param id		ID of window to move.
 * @param pos		Position to move to.
 * @return		Status code describing result of the operation. */
status_t Connection::MoveWindow(Window::ID id, org::kiwi::WindowServer::Point pos) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	Point _pos(pos.x, pos.y);
	window->MoveTo(_pos);
	return STATUS_SUCCESS;
}

/** Update an area in a window on screen.
 * @param id		ID of window to update.
 * @param rect		Rectangular area to update.
 * @return		Status code describing result of the operation. */
status_t Connection::UpdateWindow(Window::ID id, org::kiwi::WindowServer::Rect rect) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	Rect _rect(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);
	window->Update(_rect);
	return STATUS_SUCCESS;
}

/** Get the surface for a window.
 * @param id		ID of window to get surface for.
 * @param sid		Where to store surface ID.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowSurface(Window::ID id, area_id_t &sid) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	sid = window->GetSurface()->GetID();
	return STATUS_SUCCESS;
}

/** Get a window's rectangle area.
 * @param id		ID of window to get area for.
 * @param rect		Where to store rectangle.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowRect(Window::ID id, org::kiwi::WindowServer::Rect &rect) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	Rect _rect = window->GetRect();
	rect.pos.x = _rect.GetX();
	rect.pos.y = _rect.GetY();
	rect.size.width = _rect.GetWidth();
	rect.size.height = _rect.GetHeight();
	return STATUS_SUCCESS;
}

/** Get a window's title.
 * @param id		ID of window to get title for.
 * @param title		Where to store title of the window.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowTitle(Window::ID id, std::string &title) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	title = window->GetTitle();
	return STATUS_SUCCESS;
}

/** Set a window's title.
 * @param id		ID of window to set title for.
 * @param title		New title for the window.
 * @return		Status code describing result of the operation. */
status_t Connection::SetWindowTitle(Window::ID id, const std::string &title) {
	Window *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	window->SetTitle(title);
	return STATUS_SUCCESS;
}

/** Handle the connection being hung up. */
void Connection::HandleHangup() {
	/* Remove us from the session. */
	m_session->RemoveConnection(this);
	DeleteLater();
}
