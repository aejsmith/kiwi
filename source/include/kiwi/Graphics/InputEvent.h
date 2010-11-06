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
 * @brief		Input event classes.
 */

#ifndef __KIWI_GRAPHICS_INPUTEVENT_H
#define __KIWI_GRAPHICS_INPUTEVENT_H

#include <drivers/input.h>

#include <kiwi/Graphics/Point.h>
#include <kiwi/Event.h>

#include <string>

namespace kiwi {

/** Namespace containing input definitions. */
namespace Input {
	/** Keyboard modifier flags. */
	enum KeyboardModifier {
		kControlModifier = (1<<0),	/**< A Control key is pressed. */
		kAltModifier = (1<<1),		/**< An Alt key is pressed. */
		kSuperModifier = (1<<2),	/**< A Super key is pressed. */
		kShiftModifier = (1<<3),	/**< A Shift key is pressed. */
		kCapsLockModifier = (1<<4),	/**< Caps Lock is enabled. */
		kNumLockModifier = (1<<5),	/**< Num Lock is enabled. */
		kScrollLockModifier = (1<<6),	/**< Scroll Lock is enabled. */
	};

	/** Mouse button flags. */
	enum MouseButton {
		kLeftButton = (1<<0),		/**< Left button. */
		kRightButton = (1<<1),		/**< Right button. */
		kMiddleButton = (1<<2),		/**< Middle button. */
	};
}

/** Base input event class. */
class KIWI_PUBLIC InputEvent : public Event {
public:
	/** Initialise the event.
	 * @param type		Type of the event.
	 * @param time		Time at which the event occurred.
	 * @param modifiers	Keyboard modifiers that were pressed when the
	 *			event occurred. */
	InputEvent(Type type, useconds_t time, uint32_t modifiers) :
		Event(type), m_time(time), m_modifiers(modifiers)
	{}

	/** Get the time at which the event occurred.
	 * @return		The number of microseconds since the system was
	 *			booted that the event occurred at. */
	useconds_t GetTime() const { return m_time; }

	/** Get the keyboard modifiers pressed when the event occurred.
	 * @return		The set of keyboard modifiers that were pressed
	 *			when the event occurred (a bitfield of values
	 *			from Input::KeyboardModifier). */
	uint32_t GetModifiers() const { return m_modifiers; }
private:
	useconds_t m_time;		/**< Time the event occurred at. */
	uint32_t m_modifiers;		/**< Keyboard modififers. */
};

/** Mouse event class. */
class KIWI_PUBLIC MouseEvent : public InputEvent {
public:
	/** Initialise the event.
	 * @param type		Type of the event.
	 * @param time		Time at which the event occurred.
	 * @param modifiers	Keyboard modifiers that were pressed when the
	 *			event occurred.
	 * @param pos		Position of the mouse relative to the widget.
	 * @param buttons	Buttons that were pressed when the event
	 *			occurred. */
	MouseEvent(Type type, useconds_t time, uint32_t modifiers, const Point &pos, uint32_t buttons) :
		InputEvent(type, time, modifiers), m_pos(pos), m_buttons(buttons)
	{}

	/** Get the position of the mouse.
	 * @return		Position of the mouse when the event occurred.
	 *			This position is relative to the widget that
	 *			the event occurred in. */
	Point GetPosition() const { return m_pos; }

	/** Get the buttons pressed when the event occurred.
	 * @return		The set of buttons that were pressed when the
	 *			event occurred (a bitfield of values from
	 *			Input::MouseButton). For a mouse press event,
	 *			this includes the button that caused the event.
	 *			For a mouse release event, this excludes the
	 *			button that caused the event. */
	uint32_t GetButtons() const { return m_buttons; }
private:
	Point m_pos;			/**< Position of the mouse. */
	uint32_t m_buttons;		/**< Buttons that were pressed. */
};

/** Key event class. */
class KIWI_PUBLIC KeyEvent : public InputEvent {
public:
	/** Initialise the event.
	 * @param type		Type of the event.
	 * @param time		Time at which the event occurred.
	 * @param modifiers	Keyboard modifiers that were pressed when the
	 *			event occurred.
	 * @param key		Code for the key.
	 * @param text		Text that the key generated. */
	KeyEvent(Type type, useconds_t time, uint32_t modifiers, int key, const std::string &text) :
		InputEvent(type, time, modifiers), m_key(key), m_text(text)
	{}

	/** Get the code for the key.
	 * @return		The code for the key that was pressed/released
	 *			(INPUT_KEY_*). */
	int GetKey() const { return m_key; }

	/** Get the text that the key generated.
	 * @return		The text that the key generated. It takes into
	 *			account any modifiers that were pressed at time
	 *			of the event. This can be an empty string for
	 *			certain key combinations. */
	std::string GetText() const { return m_text; }
private:
	int m_key;			/**< Code for the key. */
	std::string m_text;		/**< Text that the key generated. */
};

}

#endif /* __KIWI_GRAPHICS_INPUTEVENT_H */
