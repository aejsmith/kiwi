/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#include "Connection.h"
#include "Session.h"
#include "ServerSurface.h"
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
		ServerSurface *surface = new ServerSurface(this, Size(size.width, size.height));
		m_session->AddSurface(surface);
		id = surface->GetID();
		m_surfaces.push_back(surface);
	} catch(Error &e) {
		return e.GetCode();
	}

	return STATUS_SUCCESS;
}

/** Destroy a surface.
 * @param id		ID of the surface's area.
 * @return		Status code describing result of the operation. */
status_t Connection::DestroySurface(area_id_t id) {
	ServerSurface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	} else if(surface->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	m_surfaces.remove(surface);
	m_session->RemoveSurface(surface);
	delete surface;
	return STATUS_SUCCESS;
}

/** Get the size of a surface.
 * @param id		ID of the surface's area.
 * @param size		Where to store surface size.
 * @return		Status code describing result of the operation. */
status_t Connection::GetSurfaceSize(area_id_t id, org::kiwi::WindowServer::Size &size) {
	ServerSurface *surface = m_session->FindSurface(id);
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
status_t Connection::ResizeSurface(area_id_t id, org::kiwi::WindowServer::Size size) {
	ServerSurface *surface = m_session->FindSurface(id);
	if(!surface) {
		return STATUS_NOT_FOUND;
	} else if(surface->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	return surface->Resize(Size(size.width, size.height));
}

/** Create a new window.
 * @param id		Where to store ID of window.
 * @return		Status code describing result of the operation. */
status_t Connection::CreateWindow(ServerWindow::ID &id) {
	try {
		ServerWindow *window = m_session->CreateWindow(this);
		id = window->GetID();
		m_windows.push_back(window);
	} catch(Error &e) {
		return e.GetCode();
	}

	return STATUS_SUCCESS;
}

/** Destroy a window.
 * @param id		ID of window to destroy.
 * @return		Status code describing result of the operation. */
status_t Connection::DestroyWindow(ServerWindow::ID id) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	m_windows.remove(window);
	delete window;
	return STATUS_SUCCESS;
}

status_t Connection::CloseWindow(ServerWindow::ID id) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Get a window's title.
 * @param id		ID of window to get title for.
 * @param title		Where to store title of the window.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowTitle(ServerWindow::ID id, std::string &title) {
	ServerWindow *window = m_session->FindWindow(id);
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
status_t Connection::SetWindowTitle(ServerWindow::ID id, const std::string &title) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->SetTitle(title);
	return STATUS_SUCCESS;
}

/** Get a window's style.
 * @param id		ID of window to get style of.
 * @param style		Where to store style of window.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowStyle(ServerWindow::ID id, uint32_t &style) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	style = window->GetStyle();
	return STATUS_SUCCESS;
}

/** Set a window's style.
 * @param id		ID of window to set style of.
 * @param style		New style for window.
 * @return		Status code describing result of the operation. */
status_t Connection::SetWindowStyle(ServerWindow::ID id, uint32_t style) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->SetStyle(style);
	return STATUS_SUCCESS;
}

/** Get a window's level.
 * @param id		ID of window to get level of.
 * @param level		Where to store level of window.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowLevel(ServerWindow::ID id, uint32_t &level) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	level = window->GetLevel();
	return STATUS_SUCCESS;
}

/** Set a window's level.
 * @param id		ID of window to set level of.
 * @param level		New level for window.
 * @return		Status code describing result of the operation. */
status_t Connection::SetWindowLevel(ServerWindow::ID id, uint32_t level) {
	if(level <= BaseWindow::kRootLevel || level >= BaseWindow::kCursorLevel) {
		return STATUS_INVALID_ARG;
	}

	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->SetLevel(level);
	return STATUS_SUCCESS;
}

/** Get a window's frame.
 * @param id		ID of window to get frame of.
 * @param rect		Where to store rectangle describing window's frame (the
 *			rectangular area that it occupies, relative to its
 *			parent window).
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowFrame(ServerWindow::ID id, org::kiwi::WindowServer::Rect &rect) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	Rect _rect = window->GetFrame();
	rect.pos.x = _rect.GetX();
	rect.pos.y = _rect.GetY();
	rect.size.width = _rect.GetWidth();
	rect.size.height = _rect.GetHeight();
	return STATUS_SUCCESS;
}

/** Resize a window.
 * @param id		ID of window to resize.
 * @param size		New size for the window.
 * @return		Status code describing result of the operation. */
status_t Connection::ResizeWindow(ServerWindow::ID id, org::kiwi::WindowServer::Size size) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->Resize(Size(size.width, size.height));
	return STATUS_SUCCESS;
}

/** Move a window.
 * @param id		ID of window to move.
 * @param pos		Position to move to.
 * @return		Status code describing result of the operation. */
status_t Connection::MoveWindow(ServerWindow::ID id, org::kiwi::WindowServer::Point pos) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->MoveTo(Point(pos.x, pos.y));
	return STATUS_SUCCESS;
}

/** Get a window's state.
 * @param id		ID of window to get state of.
 * @param state		Where to store state of window.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowState(ServerWindow::ID id, uint32_t &state) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	state = window->GetState();
	return STATUS_SUCCESS;
}

/** Set a window's state.
 * @param id		ID of window to set state of.
 * @param state		New state for window. Note that the kActiveState flag
 *			is ignored in this: it can only be set through
 *			ActivateWindow(), and cannot be unset by clients.
 * @return		Status code describing result of the operation. */
status_t Connection::SetWindowState(ServerWindow::ID id, uint32_t state) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	bool visible = window->IsVisible();
	window->SetState(state);
	if(!visible && (state & BaseWindow::kVisibleState)) {
		m_session->ActivateWindow(window);
	}

	return STATUS_SUCCESS;
}

/** Activate a window.
 * @param id		ID of window to activate.
 * @return		Status code describing result of the operation. */
status_t Connection::ActivateWindow(ServerWindow::ID id) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	m_session->ActivateWindow(window);
	return STATUS_SUCCESS;
}

/** Get the surface for a window.
 * @param id		ID of window to get surface for.
 * @param sid		Where to store surface ID.
 * @return		Status code describing result of the operation. */
status_t Connection::GetWindowSurface(ServerWindow::ID id, area_id_t &sid) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	}

	sid = window->GetSurface()->GetID();
	return STATUS_SUCCESS;
}

/** Update an area in a window on screen.
 * @param id		ID of window to update.
 * @param rect		Rectangular area to update.
 * @return		Status code describing result of the operation. */
status_t Connection::UpdateWindow(ServerWindow::ID id, org::kiwi::WindowServer::Rect rect) {
	ServerWindow *window = m_session->FindWindow(id);
	if(!window) {
		return STATUS_NOT_FOUND;
	} else if(window->GetOwner() != this) {
		return STATUS_ACCESS_DENIED;
	}

	window->Update(Rect(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height));
	return STATUS_SUCCESS;
}

/** Handle the connection being hung up. */
void Connection::HandleHangup() {
	/* Destroy all windows and surfaces created by the connection. */
	for(auto it = m_windows.begin(); it != m_windows.end(); ++it) {
		delete (*it);
	}
	for(auto it = m_surfaces.begin(); it != m_surfaces.end(); ++it) {
		delete (*it);
	}

	/* Remove us from the session. */
	m_session->RemoveConnection(this);
	DeleteLater();
}
