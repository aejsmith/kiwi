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
 * @brief		Base window class.
 */

#include <kiwi/Graphics/BaseWindow.h>
#include <kiwi/Graphics/Surface.h>
#include <kiwi/Error.h>

#include <cassert>
#include <map>

#include "org.kiwi.WindowServer.h"
#include "Internal.h"

using namespace kiwi;

/** Internal data for BaseWindow. */
struct kiwi::BaseWindowPrivate {
	BaseWindowPrivate();
	~BaseWindowPrivate();

	WindowServer::WindowID id;	/**< ID of the window. */
	Surface *surface;		/**< Surface for the window. */
};

/** Create a new window. */
BaseWindowPrivate::BaseWindowPrivate() : id(-1), surface(0) {
	area_id_t area;
	status_t ret;

	/* Create the window. */
	ret = WSConnection::Instance()->CreateWindow(id);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		libkiwi_warn("BaseWindow::BaseWindow: Failed to create window: %d", ret);
		throw e;
	}

	/* Get the surface for the window. */
	ret = WSConnection::Instance()->GetWindowSurface(id, area);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		libkiwi_warn("BaseWindow::BaseWindow: Failed to get window surface: %d", ret);
		throw e;
	}
	surface = new Surface(area);
}

/** Destroy the window. */
BaseWindowPrivate::~BaseWindowPrivate() {
	if(surface) {
		delete surface;
	}
	if(id >= 0) {
		WSConnection::Instance()->DestroyWindow(id);
	}
}

/** Create a new window.
 * @param style		Style of the window. This controls aspects of the
 *			window's behaviour and appearance.
 * @param level		Level of the window. This controls the order of windows
 *			on screen. Windows with higher levels always appear
 *			above windows with lower levels. */
BaseWindow::BaseWindow(uint32_t style, Level level) : m_priv(0) {
	m_priv = new BaseWindowPrivate();

	/* Put the window into the window map. */
	WSConnection::Instance().AddWindow(m_priv->id, this);

	/* Set the given settings. */
	WSConnection::Instance()->SetWindowStyle(m_priv->id, style);
	WSConnection::Instance()->SetWindowLevel(m_priv->id, level);
}

/** Create a new window.
 * @param size		Size of the window.
 * @param style		Style of the window. This controls aspects of the
 *			window's behaviour and appearance.
 * @param level		Level of the window. This controls the order of windows
 *			on screen. Windows with higher levels always appear
 *			above windows with lower levels. */
BaseWindow::BaseWindow(Size size, uint32_t style, Level level) : m_priv(0) {
	m_priv = new BaseWindowPrivate();

	/* Put the window into the window map. */
	WSConnection::Instance().AddWindow(m_priv->id, this);

	/* Set the given settings. */
	WSConnection::Instance()->SetWindowStyle(m_priv->id, style);
	WSConnection::Instance()->SetWindowLevel(m_priv->id, level);
	WindowServer::Size _size = { size.GetWidth(), size.GetHeight() };
	WSConnection::Instance()->ResizeWindow(m_priv->id, _size);
}

/** Create a new window.
 * @param frame		Position and size of the window.
 * @param style		Style of the window. This controls aspects of the
 *			window's behaviour and appearance.
 * @param level		Level of the window. This controls the order of windows
 *			on screen. Windows with higher levels always appear
 *			above windows with lower levels. */
BaseWindow::BaseWindow(Rect frame, uint32_t style, Level level) : m_priv(0) {
	m_priv = new BaseWindowPrivate();

	/* Put the window into the window map. */
	WSConnection::Instance().AddWindow(m_priv->id, this);

	/* Set the given settings. */
	WSConnection::Instance()->SetWindowStyle(m_priv->id, style);
	WSConnection::Instance()->SetWindowLevel(m_priv->id, level);
	WindowServer::Size _size = { frame.GetWidth(), frame.GetHeight() };
	WSConnection::Instance()->ResizeWindow(m_priv->id, _size);
	WindowServer::Point _pos = { frame.GetX(), frame.GetY() };
	WSConnection::Instance()->MoveWindow(m_priv->id, _pos);
}

/** Destroy the window. */
BaseWindow::~BaseWindow() {
	if(m_priv) {
		WSConnection::Instance().RemoveWindow(m_priv->id);
		delete m_priv;
	}
}

/** Get the title of the window.
 * @return		Title of the window. */
std::string BaseWindow::GetTitle() const {
	std::string title;
	WSConnection::Instance()->GetWindowTitle(m_priv->id, title);
	return title;
}

/** Set the title of the window.
 * @param title		New title for the window. */
void BaseWindow::SetTitle(const std::string &title) {
	WSConnection::Instance()->SetWindowTitle(m_priv->id, title);
}

/** Get the window's frame.
 * @return		Rectangle describing window's frame (its size and
 *			position relative to the parent window). */
Rect BaseWindow::GetFrame() const {
	WindowServer::Rect rect;
	WSConnection::Instance()->GetWindowFrame(m_priv->id, rect);
	return Rect(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height);
}

/** Resize the window.
 * @param size		New size for the window. */
void BaseWindow::Resize(Size size) {
	if(!size.IsValid()) {
		return;
	}

	WindowServer::Size _size = { size.GetWidth(), size.GetHeight() };
	WSConnection::Instance()->ResizeWindow(m_priv->id, _size);
}

/** Move the window.
 * @param pos		New position for the window. */
void BaseWindow::MoveTo(Point pos) {
	WindowServer::Point _pos = { pos.GetX(), pos.GetY() };
	WSConnection::Instance()->MoveWindow(m_priv->id, _pos);
}

/** Show the window. */
void BaseWindow::Show() {
	uint32_t state = 0;
	WSConnection::Instance()->GetWindowState(m_priv->id, state);
	if(!(state & kVisibleState)) {
		state |= kVisibleState;
		WSConnection::Instance()->SetWindowState(m_priv->id, state);
	}
}

/** Hide the window. */
void BaseWindow::Hide() {
	uint32_t state = 0;
	WSConnection::Instance()->GetWindowState(m_priv->id, state);
	if(state & kVisibleState) {
		state &= ~kVisibleState;
		WSConnection::Instance()->SetWindowState(m_priv->id, state);
	}
}

/** Check whether the window is visible.
 * @return		Whether the window is visible. */
bool BaseWindow::IsVisible() const {
	uint32_t state = 0;
	WSConnection::Instance()->GetWindowState(m_priv->id, state);
	return state & kVisibleState;
}

/** Activate the window. */
void BaseWindow::Activate() {
	WSConnection::Instance()->ActivateWindow(m_priv->id);
}

/** Check whether the window is active.
 * @return		Whether the window is active. */
bool BaseWindow::IsActive() const {
	uint32_t state = 0;
	WSConnection::Instance()->GetWindowState(m_priv->id, state);
	return state & kActiveState;
}

/** Get the window's surface.
 * @return		Pointer to surface for the window. This surface object
 *			is owned by the window: do not delete it. */
Surface *BaseWindow::GetSurface() const {
	return m_priv->surface;
}

/** Update an area in the window on screen.
 * @param rect		Rectangle to update. */
void BaseWindow::Update(Rect rect) {
	if(!rect.IsValid()) {
		return;
	}

	WindowServer::Rect _rect = { { rect.GetX(), rect.GetY() }, { rect.GetWidth(), rect.GetHeight() } };
	WSConnection::Instance()->UpdateWindow(m_priv->id, _rect);
}

/** Update an area in the window on screen.
 * @param region	Region to update. */
void BaseWindow::Update(const Region &region) {
	Region::RectArray rects;
	region.GetRects(rects);
	for(auto it = rects.begin(); it != rects.end(); ++it) {
		Update(*it);
	}
}

void BaseWindow::MouseMoved(const MouseEvent &event) {}
void BaseWindow::MousePressed(const MouseEvent &event) {}
void BaseWindow::MouseReleased(const MouseEvent &event) {}
void BaseWindow::KeyPressed(const KeyEvent &event) {}
void BaseWindow::KeyReleased(const KeyEvent &event) {}
void BaseWindow::Closed(const WindowEvent &event) {}
void BaseWindow::StateChanged(const WindowStateEvent &event) {}
void BaseWindow::TitleChanged(const WindowEvent &event) {}
void BaseWindow::Resized(const ResizeEvent &event) {}
