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
 * @brief		Graphics event classes.
 */

#ifndef __KIWI_GRAPHICS_EVENT_H
#define __KIWI_GRAPHICS_EVENT_H

#include <kiwi/Graphics/Size.h>
#include <kiwi/Event.h>

namespace kiwi {

/** Resize event class. */
class KIWI_PUBLIC ResizeEvent : public Event {
public:
	/** Initialise the event.
	 * @param size		New size of the object.
	 * @param prev		Previous size of the object. */
	ResizeEvent(Size size, Size prev) :
		Event(kResize), m_size(size), m_prev(prev)
	{}

	/** Get the new size of the object.
	 * @return		New size of the object. */
	Size GetSize() const { return m_size; }

	/** Get the previous size of the object.
	 * @return		Previous size of the object. */
	Size GetPreviousSize() const { return m_prev; }
private:
	Size m_size;			/**< New object size. */
	Size m_prev;			/**< Previous object size. */
};

/** Class for all window events. */
class KIWI_PUBLIC WindowEvent : public Event {
public:
	/** Initialise the event.
	 * @param type		Type of the event.
	 * @param id		ID of the window. */
	WindowEvent(Type type, uint32_t id) : Event(type), m_id(id) {}

	/** Get the ID of the window the event occurred on.
	 * @return		ID of the window the event occurred on. */
	uint32_t GetID() const { return m_id; }
private:
	uint32_t m_id;			/**< ID of window. */
};

/** Class for all window events. */
class KIWI_PUBLIC WindowStateEvent : public WindowEvent {
public:
	/** Initialise the event.
	 * @param id		ID of the window.
	 * @param state		New state of the window.
	 * @param prev		Previous state of the window. */
	WindowStateEvent(uint32_t id, uint32_t state, uint32_t prev) :
		WindowEvent(kWindowStateChange, id), m_state(state),
		m_prev(prev)
	{}

	/** Get the new window state.
	 * @return		New state of the window. */
	uint32_t GetState() const { return m_state; }

	/** Get the previous window state.
	 * @return		Previous state of the window. */
	uint32_t GetPreviousState() const { return m_prev; }

	bool WasShown() const;
	bool WasHidden() const;
	bool WasActivated() const;
	bool WasDeactivated() const;
	bool WasMaximized() const;
	bool WasUnmaximized() const;
	bool WasMinimized() const;
	bool WasUnminimized() const;
private:
	uint32_t m_state;		/**< New state of window. */
	uint32_t m_prev;		/**< Previous state of window. */
};

}

#endif /* __KIWI_GRAPHICS_EVENT_H */
