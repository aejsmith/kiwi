/* Kiwi shell input handling
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
 * @brief		Input handling.
 */

#include <kernel/device.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "failshell.h"

#define	L_CTRL		0x1D
#define	R_CTRL		0x1D
#define	L_ALT		0x38
#define	R_ALT		0x38
#define	L_SHIFT		0x2A
#define	R_SHIFT		0x36
#define	CAPS		0x3A

const unsigned char InputDevice::m_keymap[] = {
	0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, 0, 0,
	'#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0
};

const unsigned char InputDevice::m_keymap_shift[] = {
	0, 0x1B, '!', '"', 156, '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', 0, 0,
	'~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '|', 0, 0
};

const unsigned char InputDevice::m_keymap_caps[] = {
	0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', 39, 0, 0,
	'#', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0
};

/** Constructor for an input device.
 * @param device	Device tree path to input device. */
InputDevice::InputDevice(const char *path) : m_caps(false), m_ctrl(false), m_alt(false), m_shift(false) {
	if((m_handle = device_open(path)) < 0) {
		printf("Could not open input device (%d)\n", m_handle);
		exit(1);
	}
}

/** Get an input character.
 * @return		Character read. */
unsigned char InputDevice::getchar(void) {
	uint8_t code;
	size_t bytes;
	int ret;

	while(true) {
		if((ret = device_read(m_handle, &code, 1, 0, &bytes)) != 0) {
			printf("Failed to read input (%d)\n", ret);
			exit(1);
		} else if(bytes != 1) {
			continue;
		} else if(code >= 0xe0) {
			continue;
		}

		if(code & 0x80) {
			code &= 0x7F;
			if(code == L_SHIFT || code == R_SHIFT) {
				m_shift = false;
			} else if(code == L_CTRL || code == R_CTRL) {
				m_ctrl = false;
			} else if(code == L_ALT || code == R_ALT) {
				m_alt = 0;
			}
			continue;
		} else if(code == L_ALT || code == R_ALT) {
			m_alt = true;
		} else if(code == L_CTRL || code == R_CTRL) {
			m_ctrl = true;
		} else if(code == L_SHIFT || code == R_SHIFT) {
			m_shift = true;
		} else if(code == CAPS) {
			m_caps = !m_caps;
		} else {
			if(m_shift) {
				return m_keymap_shift[code];
			} else if(m_caps) {
				return m_keymap_caps[code];
			} else {
				return m_keymap[code];
			}
		}
	}
}
