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
