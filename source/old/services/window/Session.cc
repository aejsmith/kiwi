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

#include <assert.h>
#include <exception>
#include <iostream>

#include "Compositor.h"
#include "Connection.h"
#include "Cursor.h"
#include "Decoration.h"
#include "Display.h"
#include "Session.h"
#include "ServerSurface.h"
#include "WindowServer.h"

using namespace kiwi;
using namespace std;

/** Construct a session.
 * @param server	Server that the session is on.
 * @param id		ID of the session. */
Session::Session(WindowServer *server, session_id_t id) :
	m_server(server), m_id(id), m_active(false), m_refcount(0), m_root(0),
	m_cursor(0), m_compositor(0), m_next_wid(1), m_active_window(0),
	m_mouse_grabber(0)
{
	cairo_surface_t *image;
	cairo_t *context;
	int w, h;

	/* Create the root window. */
	Rect frame(Point(0, 0), m_server->GetDisplay()->GetSize());
	m_root = new ServerWindow(this, 0, 0, 0, BaseWindow::kActivatableMask, BaseWindow::kRootLevel, frame);
	m_active_window = m_root;

	/* Set up a Cairo context for rendering on to the root surface. */
	context = cairo_create(m_root->GetSurface()->GetCairoSurface());
	if(cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(context)) << endl;
		throw exception();
	}

	/* Load the background image. */
	image = cairo_image_surface_create_from_png("/system/data/images/wallpaper.png");
	if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to load background image: ";
		clog << cairo_status_to_string(cairo_surface_status(image)) << endl;
		throw exception();
	}

	/* Draw the background image, scaling it if necessary. */
	w = cairo_image_surface_get_width(image);
	h = cairo_image_surface_get_height(image);
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

/** Destroy a session. */
Session::~Session() {
	delete m_cursor;
	delete m_compositor;
	delete m_root;
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
void Session::AddSurface(ServerSurface *surface) {
	m_surfaces.insert(make_pair(surface->GetID(), surface));
}

/** Remove a surface from the session.
 * @param surface	Surface to remove. */
void Session::RemoveSurface(ServerSurface *surface) {
	m_surfaces.erase(surface->GetID());
}

/** Find a surface.
 * @param id		ID of surface to find.
 * @return		Pointer to surface if found, NULL if not. */
ServerSurface *Session::FindSurface(area_id_t id) {
	SurfaceMap::iterator it = m_surfaces.find(id);
	return (it != m_surfaces.end()) ? it->second : 0;
}

/** Create a new window.
 * @param owner		Connection creating the window.
 * @return		Pointer to window. */
ServerWindow *Session::CreateWindow(Connection *owner) {
	ServerWindow *window = new ServerWindow(this, m_next_wid++, m_root, owner,
	                                        BaseWindow::kNormalStyle,
	                                        BaseWindow::kNormalLevel,
	                                        Rect(10, 35, 100, 100));
	m_windows.insert(make_pair(window->GetID(), window));
	return window;
}

/** Remove a window from the session.
 * @param window	Window to remove. */
void Session::RemoveWindow(ServerWindow *window) {
	m_windows.erase(window->GetID());
	if(m_mouse_grabber == window || m_mouse_grabber == window->GetDecoration()) {
		m_mouse_grabber = 0;
	}
	if(m_active_window == window) {
		ActivateWindow(m_root);
	}
}

/** Find a window.
 * @param id		ID of window to find.
 * @return		Pointer to window if found, NULL if not. */
ServerWindow *Session::FindWindow(ServerWindow::ID id) {
	WindowMap::iterator it = m_windows.find(id);
	return (it != m_windows.end()) ? it->second : 0;
}

/** Get the window under the cursor.
 * @return		Pointer to window under the cursor. */
ServerWindow *Session::WindowAtCursor() {
	return m_root->AtPosition(m_cursor->GetPosition());
}

/** Set a window as the active window.
 * @param window	Window to set as active. */
void Session::ActivateWindow(ServerWindow *window) {
	if(m_active_window != window) {
		m_active_window->SetActive(false);
		m_active_window = window;
		m_active_window->SetVisible(true);
		m_active_window->SetActive(true);
	}
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

/** Dispatch a mouse move event.
 * @param time		Time of the event.
 * @param dx		X position delta.
 * @param dy		Y position delta.
 * @param modifiers	Keyboard modifiers pressed at time of the event.
 * @param buttons	Buttons pressed at time of the event. */
void Session::MouseMoved(useconds_t time, int dx, int dy, uint32_t modifiers, uint32_t buttons) {
	/* Move the cursor. */
	m_cursor->MoveRelative(dx, dy);

	/* Get the target for the event. */
	Point pos;
	MouseReceiver *object = MouseEventTarget(pos);

	/* Send the event. */
	MouseEvent event(Event::kMouseMove, time, modifiers, pos, buttons);
	object->MouseMoved(event);
}

/** Dispatch a mouse press event.
 * @param time		Time of the event.
 * @param modifiers	Keyboard modifiers pressed at time of the event.
 * @param buttons	Buttons pressed at time of the event. */
void Session::MousePressed(useconds_t time, uint32_t modifiers, uint32_t buttons) {
	/* Get the target for the event. If it is a window, activate it. */
	Point pos;
	MouseReceiver *object = MouseEventTarget(pos, true);

	/* Send the event. */
	MouseEvent event(Event::kMousePress, time, modifiers, pos, buttons);
	object->MousePressed(event);
}

/** Dispatch a mouse release event.
 * @param time		Time of the event.
 * @param modifiers	Keyboard modifiers pressed at time of the event.
 * @param buttons	Buttons pressed at time of the event. */
void Session::MouseReleased(useconds_t time, uint32_t modifiers, uint32_t buttons) {
	/* Get the target for the event. If it is a window, activate it. */
	Point pos;
	MouseReceiver *object = MouseEventTarget(pos);

	/* Send the event. */
	MouseEvent event(Event::kMouseRelease, time, modifiers, pos, buttons);
	object->MouseReleased(event);
}

/** Dispatch a key press event.
 * @param event		Key event object. */
void Session::KeyPressed(const KeyEvent &event) {
	m_active_window->KeyPressed(event);
}

/** Dispatch a key release event.
 * @param event		Key release object. */
void Session::KeyReleased(const KeyEvent &event) {
	m_active_window->KeyReleased(event);
}

/** Grab the mouse, causing all mouse events to be sent to the object.
 * @param object	Mouse receiver to sent events to. */
void Session::GrabMouse(MouseReceiver *object) {
	m_mouse_grabber = object;
}

/** Release the mouse. */
void Session::ReleaseMouse() {
	m_mouse_grabber = 0;
}

/** Work out the target for a mouse event.
 * @param pos		Where to store relative position for event.
 * @param activate	If looking up a window, whether to activate it.
 * @return		Pointer to receiver. */
MouseReceiver *Session::MouseEventTarget(Point &pos, bool activate) {
	Point abs = m_cursor->GetPosition();
	if(m_mouse_grabber) {
		pos = m_mouse_grabber->RelativePoint(abs);
		return m_mouse_grabber;
	} else {
		/* Use the window that the cursor is currently pointing at. */
		ServerWindow *window = m_root->AtPosition(abs);
		pos = window->RelativePoint(abs);
		if(activate) {
			ActivateWindow(window);
		}
		return window;
	}
}

/** Decrease the session reference count. */
void Session::Release() {
	if(--m_refcount == 0) {
		m_server->RemoveSession(this);
		DeleteLater();
	}
}