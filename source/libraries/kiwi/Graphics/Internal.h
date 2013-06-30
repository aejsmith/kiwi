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
 * @brief		Internal libkiwi graphics definitions.
 */

#ifndef __GRAPHICS_INTERNAL_H
#define __GRAPHICS_INTERNAL_H

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Support/Noncopyable.h>

#include <map>

#include "org.kiwi.WindowServer.h"
#include "../Internal.h"

using namespace org::kiwi;

namespace kiwi {

/** Class for interfacing with the window server. */
class WSConnection : Noncopyable {
	/** Type of the window map. */
	typedef std::map<WindowServer::WindowID, BaseWindow *> WindowMap;
public:
	void AddWindow(WindowServer::WindowID id, BaseWindow *window);
	void RemoveWindow(WindowServer::WindowID id);
	BaseWindow *FindWindow(WindowServer::WindowID id);

	/** Access the server connection.
	 * @return		Pointer to server connection. */
	WindowServer::ServerConnection *operator ->() {
		return m_conn;
	}

	static WSConnection &Instance();
private:
	WSConnection();
	~WSConnection();

	void OnMouseMove(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
	                 WindowServer::Point pos, uint32_t buttons);
	void OnMousePress(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
	                  WindowServer::Point pos, uint32_t buttons);
	void OnMouseRelease(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
	                    WindowServer::Point pos, uint32_t buttons);
	void OnKeyPress(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
	                int32_t key, const std::string &text);
	void OnKeyRelease(WindowServer::WindowID id, int64_t time, uint32_t modifiers,
	                  int32_t key, const std::string &text);
	void OnWindowClose(WindowServer::WindowID id);
	void OnWindowTitleChange(WindowServer::WindowID id);
	void OnWindowStateChange(WindowServer::WindowID id, uint32_t state, uint32_t prev);
	void OnWindowResize(WindowServer::WindowID id, WindowServer::Size size,
	                    WindowServer::Size prev);

	WindowServer::ServerConnection *m_conn;
	WindowMap m_windows;
};

}

#endif /* __GRAPHICS_INTERNAL_H */
