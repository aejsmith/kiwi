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
 * @brief		Display class.
 */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <drivers/display.h>

#include <kiwi/Handle.h>

#include <vector>

/** Class representing a display. */
class Display : public kiwi::Handle {
public:
	/** Type of the mode vector. */
	typedef std::vector<display_mode_t> ModeVector;

	Display(const char *path);
	~Display();

	/** Get an array of modes supported by the device.
	 * @return		Reference to mode vector. */
	const ModeVector &GetModes() const { return m_modes; }

	/** Get the current mode the device is using.
	 * @return		Reference to current mode. */
	const display_mode_t &GetCurrentMode() const { return m_current_mode; }
private:
	void RegisterEvents();
	void EventReceived(int id);

	ModeVector m_modes;		/**< Modes supported by the device. */
	display_mode_t m_current_mode;	/**< Current mode set on the device. */
};

#endif /* __DISPLAY_H */
