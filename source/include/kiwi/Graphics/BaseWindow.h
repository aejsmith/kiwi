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

#ifndef __KIWI_GRAPHICS_BASEWINDOW_H
#define __KIWI_GRAPHICS_BASEWINDOW_H

#include <kiwi/Graphics/Event.h>
#include <kiwi/Graphics/InputEvent.h>
#include <kiwi/Graphics/Rect.h>
#include <kiwi/Graphics/Region.h>

#include <kiwi/Support/Noncopyable.h>

#include <kiwi/Object.h>

#include <string>

namespace kiwi {

class BaseWindowPrivate;
class WSConnection;
class Surface;

/** Base window class.
 *
 * This class provides an interface to the window server. It only provides
 * users with a surface to render to and input events. It does not handle
 * things like UI widgets: for this, use the Window class.
 */
class KIWI_PUBLIC BaseWindow : public kiwi::Object, kiwi::Noncopyable {
	friend class WSConnection;
public:
	/** Window levels.
	 * @note		kRootLevel and kCursorLevel cannot be set by
	 *			applications. */
	enum Level : uint32_t {
		kRootLevel = 0,			/**< Root window level. */
		kNormalLevel = 10,		/**< Normal window level. */
		kPanelLevel = 20,		/**< Panel level. */
		kPopupLevel = 30,		/**< Popup (menu, tooltip) level. */
		kCursorLevel = 100,		/**< Cursor level. */
	};

	/** Window style flags. */
	enum : uint32_t {
		kActivatableMask = (1<<0),	/**< Can be made the active window. */
		kBorderMask = (1<<1),		/**< Has a border. */
		kMinimizableMask = (1<<2),	/**< The window can be minimized. */
		kMaximizableMask = (1<<3),	/**< The window can be maximized. */
		kResizableMask = (1<<4),	/**< The window can be resized by the user. */
		kMovableMask = (1<<5),		/**< The window can be moved by the user. */
	};

	/** Pre-defined window styles. */
	enum : uint32_t {
		/** Normal window with a border. */
		kNormalStyle = (kActivatableMask | kBorderMask | kMinimizableMask |
		                kMaximizableMask | kResizableMask | kMovableMask),

		/** Window with no border. */
		kBorderlessStyle = (kActivatableMask | kMinimizableMask | kMaximizableMask |
		                    kResizableMask | kMovableMask),
	};

	/** Window state flags. */
	enum : uint32_t {
		kVisibleState = (1<<0),		/**< Window is visible. */
		kActiveState = (1<<1),		/**< Window is active (cannot be changed through SetState()). */
		kMaximizedState = (1<<2),	/**< Window is maximized. */
		kMinimizedState = (1<<3),	/**< Window is minimized. */
	};

	BaseWindow(uint32_t style = kNormalStyle, Level level = kNormalLevel);
	BaseWindow(const Size &size, uint32_t style = kNormalStyle, Level level = kNormalLevel);
	BaseWindow(const Rect &frame, uint32_t style = kNormalStyle, Level level = kNormalLevel);
	~BaseWindow();

	std::string GetTitle() const;
	void SetTitle(const std::string &title);
	Rect GetFrame() const;
	void Resize(const Size &size);
	void MoveTo(const Point &pos);
	void Show();
	void Hide();
	bool IsVisible() const;
	void Activate();
	bool IsActive() const;
	Surface *GetSurface() const;
	void Update(const Rect &rect);
	void Update(const Region &region);
protected:
	virtual void MouseMoved(const MouseEvent &event);
	virtual void MousePressed(const MouseEvent &event);
	virtual void MouseReleased(const MouseEvent &event);
	virtual void KeyPressed(const KeyEvent &event);
	virtual void KeyReleased(const KeyEvent &event);
	virtual void Closed(const WindowEvent &event);
	virtual void StateChanged(const WindowStateEvent &event);
	virtual void TitleChanged(const WindowEvent &event);
	virtual void Resized(const ResizeEvent &event);
private:
	BaseWindowPrivate *m_priv;	/**< Internal data pointer. */
};

}

#endif /* __KIWI_GRAPHICS_BASEWINDOW_H */
