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

#include <exception>
#include <iostream>

#include "Compositor.h"
#include "Connection.h"
#include "Cursor.h"
#include "Display.h"
#include "Session.h"
#include "Surface.h"
#include "WindowServer.h"

using namespace kiwi;
using namespace std;

/** Construct a session.
 * @param server	Server that the session is on.
 * @param id		ID of the session. */
Session::Session(WindowServer *server, session_id_t id) :
	m_server(server), m_id(id), m_active(false), m_refcount(0),
	m_next_window_id(1), m_active_window(0)
{
	/* Create the root window. */
	Rect rect(0, 0, m_server->GetDisplay()->GetCurrentMode().width, m_server->GetDisplay()->GetCurrentMode().height);
	m_root = new Window(this, 0, 0, rect, WINDOW_TYPE_ROOT);
	m_active_window = m_root;

	/* Set up a Cairo context for rendering on to the root surface. */
	cairo_t *context = cairo_create(m_root->GetSurface()->GetCairoSurface());
	if(cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(context)) << endl;
		throw exception();
	}

	/* Load the background image. */
	cairo_surface_t *image = cairo_image_surface_create_from_png("/system/data/images/wallpaper.png");
	if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to load background image: ";
		clog << cairo_status_to_string(cairo_surface_status(image)) << endl;
		throw exception();
	}

	/* Draw the background image, scaling it if necessary. */
	int w = cairo_image_surface_get_width(image);
	int h = cairo_image_surface_get_height(image);
	cairo_scale(context, static_cast<double>(m_root->GetSurface()->GetWidth()) / w,
	            static_cast<double>(m_root->GetSurface()->GetHeight()) / h);
	cairo_set_source_surface(context, image, 0, 0);
	cairo_paint(context);
	cairo_destroy(context);
	cairo_surface_destroy(image);

	/* Create the compositor. */
	m_compositor = new Compositor(m_server->GetDisplay(), m_root);

	/* Create the cursor. */
	m_cursor = new Cursor(this);
}

/** Handle a connection from a process in the session.
 * @param handle	Handle to the connection. */
void Session::HandleConnection(handle_t handle) {
	m_refcount++;
	m_connections.push_back(new Connection(this, handle));
}

/** Remove a connection from the session.
 * @param conn		Connection to remove. */
void Session::RemoveConnection(Connection *conn) {
	m_connections.remove(conn);
	Release();
}

/** Add a surface to the session.
 * @param surface	Surface to add. */
void Session::AddSurface(Surface *surface) {
	m_surfaces.insert(make_pair(surface->GetID(), surface));
}

/** Remove a surface from the session.
 * @param surface	Surface to remove. */
void Session::RemoveSurface(Surface *surface) {
	m_surfaces.erase(surface->GetID());
}

/** Find a surface.
 * @param id		ID of surface to find.
 * @return		Pointer to surface if found, NULL if not. */
Surface *Session::FindSurface(area_id_t id) {
	SurfaceMap::iterator it = m_surfaces.find(id);
	return (it != m_surfaces.end()) ? it->second : 0;
}

/** Create a new window.
 * @param rect		Rectangle area for the window.
 * @return		Pointer to window. */
Window *Session::CreateWindow(Rect &rect) {
	// FIXME type and parent arg
	Window *window = new Window(this, m_next_window_id++, m_root, rect, WINDOW_TYPE_NORMAL);
	m_windows.insert(make_pair(window->GetID(), window));
	return window;
}

/** Remove a window from the session.
 * @param window	Window to remove. */
void Session::RemoveWindow(Window *window) {
	m_windows.erase(window->GetID());
	if(m_active_window == window) {
		ActivateWindow(m_root);
	}
}

/** Find a window.
 * @param id		ID of window to find.
 * @return		Pointer to window if found, NULL if not. */
Window *Session::FindWindow(Window::ID id) {
	WindowMap::iterator it = m_windows.find(id);
	return (it != m_windows.end()) ? it->second : 0;
}

/** Activate a window.
 * @param window	Window to activate. */
void Session::ActivateWindow(Window *window) {
	if(m_active_window != window) {
		if(m_active_window) {
			m_active_window->SetActive(false);
		}

		m_active_window = window;
		m_active_window->SetVisible(true);
		m_active_window->SetActive(true);
	}
}

/** Hide a window.
 * @param window	Window to hide. */
void Session::HideWindow(Window *window) {
	if(m_active_window == window) {
		ActivateWindow(m_root);
	}
	window->SetVisible(false);
}

/** Make the session the active session. */
void Session::Activate() {
	m_refcount++;
	m_active = true;
	m_compositor->Redraw();
}

/** Deactivate the session. */
void Session::Deactivate() {
	m_active = false;
	Release();
}

/** Decrease the session reference count. */
void Session::Release() {
	if(--m_refcount == 0) {
		m_server->RemoveSession(this);
		DeleteLater();
	}
}
