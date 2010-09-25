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
 * @brief		i8042 keycode translation table.
 */

#include <drivers/input.h>

/** Table converting i8042 keyboard codes to input layer codes. */
int32_t i8042_keycode_table[128][2] = {
	{ 0, 0 },
	{ INPUT_KEY_ESC, 0 },
	{ INPUT_KEY_1, 0 },
	{ INPUT_KEY_2, 0 },
	{ INPUT_KEY_3, 0 },
	{ INPUT_KEY_4, 0 },
	{ INPUT_KEY_5, 0 },
	{ INPUT_KEY_6, 0 },
	{ INPUT_KEY_7, 0 },
	{ INPUT_KEY_8, 0 },
	{ INPUT_KEY_9, 0 },
	{ INPUT_KEY_0, 0 },
	{ INPUT_KEY_MINUS, 0 },
	{ INPUT_KEY_EQUAL, 0 },
	{ INPUT_KEY_BACKSPACE, 0 },
	{ INPUT_KEY_TAB, 0 },
	{ INPUT_KEY_Q, 0 },
	{ INPUT_KEY_W, 0 },
	{ INPUT_KEY_E, 0 },
	{ INPUT_KEY_R, 0 },
	{ INPUT_KEY_T, 0 },
	{ INPUT_KEY_Y, 0 },
	{ INPUT_KEY_U, 0 },
	{ INPUT_KEY_I, 0 },
	{ INPUT_KEY_O, 0 },
	{ INPUT_KEY_P, 0 },
	{ INPUT_KEY_LBRACE, 0 },
	{ INPUT_KEY_RBRACE, 0 },
	{ INPUT_KEY_ENTER, 0 },
	{ INPUT_KEY_LCTRL, 0 },
	{ INPUT_KEY_A, 0 },
	{ INPUT_KEY_S, 0 },
	{ INPUT_KEY_D, 0 },
	{ INPUT_KEY_F, 0 },
	{ INPUT_KEY_G, 0 },
	{ INPUT_KEY_H, 0 },
	{ INPUT_KEY_J, 0 },
	{ INPUT_KEY_K, 0 },
	{ INPUT_KEY_L, 0 },
	{ INPUT_KEY_SEMICOLON, 0 },
	{ INPUT_KEY_APOSTROPHE, 0 },
	{ INPUT_KEY_GRAVE, 0 },
	{ INPUT_KEY_LSHIFT, 0 },
	{ INPUT_KEY_BACKSLASH, 0 },
	{ INPUT_KEY_Z, 0 },
	{ INPUT_KEY_X, 0 },
	{ INPUT_KEY_C, 0 },
	{ INPUT_KEY_V, 0 },
	{ INPUT_KEY_B, 0 },
	{ INPUT_KEY_N, 0 },
	{ INPUT_KEY_M, 0 },
	{ INPUT_KEY_COMMA, 0 },
	{ INPUT_KEY_PERIOD, 0 },
	{ INPUT_KEY_SLASH, 0 },
	{ INPUT_KEY_RSHIFT, 0 },
	{ INPUT_KEY_KPASTERISK, 0 },
	{ INPUT_KEY_LALT, 0 },
	{ INPUT_KEY_SPACE, 0 },
	{ INPUT_KEY_CAPSLOCK, 0 },
	{ INPUT_KEY_F1, 0 },
	{ INPUT_KEY_F2, 0 },
	{ INPUT_KEY_F3, 0 },
	{ INPUT_KEY_F4, 0 },
	{ INPUT_KEY_F5, 0 },
	{ INPUT_KEY_F6, 0 },
	{ INPUT_KEY_F7, 0 },
	{ INPUT_KEY_F8, 0 },
	{ INPUT_KEY_F9, 0 },
	{ INPUT_KEY_F10, 0 },
	{ INPUT_KEY_NUMLOCK, 0 },
	{ INPUT_KEY_SCROLLLOCK, 0 },
	{ INPUT_KEY_KP7, INPUT_KEY_HOME },
	{ INPUT_KEY_KP8, INPUT_KEY_UP },
	{ INPUT_KEY_KP9, INPUT_KEY_PGUP },
	{ INPUT_KEY_KPMINUS, 0 },
	{ INPUT_KEY_KP4, INPUT_KEY_LEFT },
	{ INPUT_KEY_KP5, 0 },
	{ INPUT_KEY_KP6, INPUT_KEY_RIGHT },
	{ INPUT_KEY_KPPLUS, 0 },
	{ INPUT_KEY_KP1, INPUT_KEY_END },
	{ INPUT_KEY_KP2, INPUT_KEY_DOWN },
	{ INPUT_KEY_KP3, INPUT_KEY_PGDOWN },
	{ INPUT_KEY_KP0, INPUT_KEY_INSERT },
	{ INPUT_KEY_KPPERIOD, INPUT_KEY_DELETE },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ INPUT_KEY_F11, 0 },
	{ INPUT_KEY_F12, 0 },
};
