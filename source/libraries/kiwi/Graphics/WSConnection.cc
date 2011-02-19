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

#include <kiwi/Graphics/Surface.h>

#include "Internal.h"

using namespace kiwi;

/** Global instance of WSConnection. */
static WSConnection *g_wsconn_instance = 0;

/** Set up the window server connection. */
WSConnection::WSConnection() : m_conn(0) {
	m_conn = new WindowServer::ServerConnection();
	m_conn->OnMouseMove.Connect(this, &WSConnection::OnMouseMove);
	m_conn->OnMousePress.Connect(this, &WSConnection::OnMousePress);
	m_conn->OnMouseRelease.Connect(this, &WSConnection::OnMouseRelease);
	m_conn->OnKeyPress.Connect(this, &WSConnection::OnKeyPress);
	m_conn->OnKeyRelease.Connect(this, &WSConnection::OnKeyRelease);
	m_conn->OnWindowClose.Connect(this, &WSConnection::OnWindowClose);
	m_conn->OnWindowTitleChange.Connect(this, &WSConnection::OnWindowTitleChange);
	m_conn->OnWindowStateChange.Connect(this, &WSConnection::OnWindowStateChange);
	m_conn->OnWindowResize.Connect(this, &WSConnection::OnWindowResize);
	m_conn->Connect();
}

/** Destroy the server connection. */
WSConnection::~WSConnection() {
	delete m_conn;
}

/** Get the instance of WSConnection.
 * @return		Reference to connection instance. */
WSConnection &WSConnection::Instance() {
	if(!g_wsconn_instance) {
		g_wsconn_instance = new WSConnection();
	}

	return *g_wsconn_instance;
}

/** Add a window to the window map.
 * @param id		ID of window to add.
 * @param window	Window to add. */
void WSConnection::AddWindow(WindowServer::WindowID id, BaseWindow *window) {
	m_windows.insert(std::make_pair(id, window));
}

/** Remove a window from the window map.
 * @param id		ID of window to remove. */
void WSConnection::RemoveWindow(WindowServer::WindowID id) {
	m_windows.erase(id);
}

/** Get a window from the window map.
 * @param id		ID of the window.
 * @return		Pointer to window. A fatal error is raised if the
 *			window does not exist in the window map. */
BaseWindow *WSConnection::FindWindow(WindowServer::WindowID id) {
	WindowMap::iterator it = m_windows.find(id);
	if(it == m_windows.end()) {
		libkiwi_fatal("Received window event from unknown window.");
	}

	return it->second;
}

/** Handle a mouse move event. */
void WSConnection::OnMouseMove(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
                               WindowServer::Point pos, uint32_t buttons) {
	MouseEvent event(Event::kMouseMove, time, modifiers, Point(pos.x, pos.y), buttons);
	BaseWindow *window = FindWindow(id);
	window->MouseMoved(event);
}

/** Handle a mouse press event. */
void WSConnection::OnMousePress(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
                                     WindowServer::Point pos, uint32_t buttons) {
	MouseEvent event(Event::kMousePress, time, modifiers, Point(pos.x, pos.y), buttons);
	BaseWindow *window = FindWindow(id);
	window->MousePressed(event);
}

/** Handle a mouse release event. */
void WSConnection::OnMouseRelease(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
                                  WindowServer::Point pos, uint32_t buttons) {
	MouseEvent event(Event::kMouseRelease, time, modifiers, Point(pos.x, pos.y), buttons);
	BaseWindow *window = FindWindow(id);
	window->MouseReleased(event);
}

/** Handle a key press event. */
void WSConnection::OnKeyPress(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
                              int32_t key, const std::string &text) {
	KeyEvent event(Event::kKeyPress, time, modifiers, key, text);
	BaseWindow *window = FindWindow(id);
	window->KeyPressed(event);
}

/** Handle a key release event. */
void WSConnection::OnKeyRelease(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
                                int32_t key, const std::string &text) {
	KeyEvent event(Event::kKeyRelease, time, modifiers, key, text);
	BaseWindow *window = FindWindow(id);
	window->KeyReleased(event);
}

/** Handle a window close event. */
void WSConnection::OnWindowClose(WindowServer::WindowID id) {
	WindowEvent event(Event::kWindowClose, id);
	BaseWindow *window = FindWindow(id);
	window->Closed(event);
}

/** Handle a window title change event. */
void WSConnection::OnWindowTitleChange(WindowServer::WindowID id) {
	WindowEvent event(Event::kWindowTitleChange, id);
	BaseWindow *window = FindWindow(id);
	window->TitleChanged(event);
}

/** Handle a window state change event. */
void WSConnection::OnWindowStateChange(WindowServer::WindowID id, uint32_t state, uint32_t prev) {
	WindowStateEvent event(id, state, prev);
	BaseWindow *window = FindWindow(id);
	window->StateChanged(event);
}

/** Handle a window resize event. */
void WSConnection::OnWindowResize(WindowServer::WindowID id, WindowServer::Size size,
                                  WindowServer::Size prev) {
	BaseWindow *window = FindWindow(id);

	/* Resize the window's surface. */
	Surface *surface = window->GetSurface();
	if(!surface->Resize(Size(size.width, size.height))) {
		libkiwi_fatal("Failed to resize window surface");
	}

	/* Send the event to the window. */
	ResizeEvent event(Size(size.width, size.height), Size(prev.width, prev.height));
	window->Resized(event);
}
