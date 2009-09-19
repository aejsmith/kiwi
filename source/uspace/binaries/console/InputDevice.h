/* Kiwi console input handling
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Console input handling.
 */

#ifndef __INPUTDEVICE_H
#define __INPUTDEVICE_H

#include <kernel/handle.h>

/** Input device class. */
class InputDevice {
public:
	/** Constructor for an InputDevice object.
	 * @param path		Device path to use. */
	InputDevice(const char *path);
	~InputDevice();

	/** Check if initialisation succeeded.
	 * @return		0 if succeeded, negative error code if not. */
	int InitCheck(void) const { return m_init_status; }
private:
	/** Thread function.
	 * @param arg		Thread argument (device object pointer). */
	static void _ThreadEntry(void *arg);

	static const unsigned char m_keymap[];
	static const unsigned char m_keymap_shift[];
	static const unsigned char m_keymap_caps[];

	int m_init_status;		/**< Initialisation status. */
	handle_t m_device;		/**< Handle to device. */
	handle_t m_thread;		/**< Handle to thread. */
	bool m_caps;			/**< Whether Caps Lock is on. */
	bool m_ctrl;			/**< Whether Ctrl is held. */
	bool m_alt;			/**< Whether Alt is held. */
	bool m_shift;			/**< Whether Shift is held. */
};

#endif /* __INPUTDEVICE_H */
