/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               i8042 keyboard codes.
 */

#include <device/input/input.h>

/**
 * Table converting i8042 keyboard codes to input event key codes. Indexed by
 * code and whether prefixed with 0xe0 (extended codes). 0xe1 is handled
 * specially.
 */
int32_t i8042_keycode_table[128][2] = {
    /* 0x00 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x01 */ { INPUT_KEY_ESCAPE,              INPUT_KEY_UNKNOWN },
    /* 0x02 */ { INPUT_KEY_1,                   INPUT_KEY_UNKNOWN },
    /* 0x03 */ { INPUT_KEY_2,                   INPUT_KEY_UNKNOWN },
    /* 0x04 */ { INPUT_KEY_3,                   INPUT_KEY_UNKNOWN },
    /* 0x05 */ { INPUT_KEY_4,                   INPUT_KEY_UNKNOWN },
    /* 0x06 */ { INPUT_KEY_5,                   INPUT_KEY_UNKNOWN },
    /* 0x07 */ { INPUT_KEY_6,                   INPUT_KEY_UNKNOWN },
    /* 0x08 */ { INPUT_KEY_7,                   INPUT_KEY_UNKNOWN },
    /* 0x09 */ { INPUT_KEY_8,                   INPUT_KEY_UNKNOWN },
    /* 0x0a */ { INPUT_KEY_9,                   INPUT_KEY_UNKNOWN },
    /* 0x0b */ { INPUT_KEY_0,                   INPUT_KEY_UNKNOWN },
    /* 0x0c */ { INPUT_KEY_MINUS,               INPUT_KEY_UNKNOWN },
    /* 0x0d */ { INPUT_KEY_EQUALS,              INPUT_KEY_UNKNOWN },
    /* 0x0e */ { INPUT_KEY_BACKSPACE,           INPUT_KEY_UNKNOWN },
    /* 0x0f */ { INPUT_KEY_TAB,                 INPUT_KEY_UNKNOWN },
    /* 0x10 */ { INPUT_KEY_Q,                   INPUT_KEY_UNKNOWN },
    /* 0x11 */ { INPUT_KEY_W,                   INPUT_KEY_UNKNOWN },
    /* 0x12 */ { INPUT_KEY_E,                   INPUT_KEY_UNKNOWN },
    /* 0x13 */ { INPUT_KEY_R,                   INPUT_KEY_UNKNOWN },
    /* 0x14 */ { INPUT_KEY_T,                   INPUT_KEY_UNKNOWN },
    /* 0x15 */ { INPUT_KEY_Y,                   INPUT_KEY_UNKNOWN },
    /* 0x16 */ { INPUT_KEY_U,                   INPUT_KEY_UNKNOWN },
    /* 0x17 */ { INPUT_KEY_I,                   INPUT_KEY_UNKNOWN },
    /* 0x18 */ { INPUT_KEY_O,                   INPUT_KEY_UNKNOWN },
    /* 0x19 */ { INPUT_KEY_P,                   INPUT_KEY_UNKNOWN },
    /* 0x1a */ { INPUT_KEY_LEFT_BRACKET,        INPUT_KEY_UNKNOWN },
    /* 0x1b */ { INPUT_KEY_RIGHT_BRACKET,       INPUT_KEY_UNKNOWN },
    /* 0x1c */ { INPUT_KEY_RETURN,              INPUT_KEY_UNKNOWN },
    /* 0x1d */ { INPUT_KEY_LEFT_CTRL,           INPUT_KEY_RIGHT_CTRL },
    /* 0x1e */ { INPUT_KEY_A,                   INPUT_KEY_UNKNOWN },
    /* 0x1f */ { INPUT_KEY_S,                   INPUT_KEY_UNKNOWN },
    /* 0x20 */ { INPUT_KEY_D,                   INPUT_KEY_UNKNOWN },
    /* 0x21 */ { INPUT_KEY_F,                   INPUT_KEY_UNKNOWN },
    /* 0x22 */ { INPUT_KEY_G,                   INPUT_KEY_UNKNOWN },
    /* 0x23 */ { INPUT_KEY_H,                   INPUT_KEY_UNKNOWN },
    /* 0x24 */ { INPUT_KEY_J,                   INPUT_KEY_UNKNOWN },
    /* 0x25 */ { INPUT_KEY_K,                   INPUT_KEY_UNKNOWN },
    /* 0x26 */ { INPUT_KEY_L,                   INPUT_KEY_UNKNOWN },
    /* 0x27 */ { INPUT_KEY_SEMICOLON,           INPUT_KEY_UNKNOWN },
    /* 0x28 */ { INPUT_KEY_APOSTROPHE,          INPUT_KEY_UNKNOWN },
    /* 0x29 */ { INPUT_KEY_GRAVE,               INPUT_KEY_UNKNOWN },
    /* 0x2a */ { INPUT_KEY_LEFT_SHIFT,          INPUT_KEY_UNKNOWN },
    /* 0x2b */ { INPUT_KEY_BACKSLASH,           INPUT_KEY_UNKNOWN },
    /* 0x2c */ { INPUT_KEY_Z,                   INPUT_KEY_UNKNOWN },
    /* 0x2d */ { INPUT_KEY_X,                   INPUT_KEY_UNKNOWN },
    /* 0x2e */ { INPUT_KEY_C,                   INPUT_KEY_UNKNOWN },
    /* 0x2f */ { INPUT_KEY_V,                   INPUT_KEY_UNKNOWN },
    /* 0x30 */ { INPUT_KEY_B,                   INPUT_KEY_UNKNOWN },
    /* 0x31 */ { INPUT_KEY_N,                   INPUT_KEY_UNKNOWN },
    /* 0x32 */ { INPUT_KEY_M,                   INPUT_KEY_UNKNOWN },
    /* 0x33 */ { INPUT_KEY_COMMA,               INPUT_KEY_UNKNOWN },
    /* 0x34 */ { INPUT_KEY_PERIOD,              INPUT_KEY_UNKNOWN },
    /* 0x35 */ { INPUT_KEY_SLASH,               INPUT_KEY_UNKNOWN },
    /* 0x36 */ { INPUT_KEY_RIGHT_SHIFT,         INPUT_KEY_UNKNOWN },
    /* 0x37 */ { INPUT_KEY_KP_ASTERISK,         INPUT_KEY_PRINT_SCREEN },
    /* 0x38 */ { INPUT_KEY_LEFT_ALT,            INPUT_KEY_RIGHT_ALT },
    /* 0x39 */ { INPUT_KEY_SPACE,               INPUT_KEY_UNKNOWN },
    /* 0x3a */ { INPUT_KEY_CAPS_LOCK,           INPUT_KEY_UNKNOWN },
    /* 0x3b */ { INPUT_KEY_F1,                  INPUT_KEY_UNKNOWN },
    /* 0x3c */ { INPUT_KEY_F2,                  INPUT_KEY_UNKNOWN },
    /* 0x3d */ { INPUT_KEY_F3,                  INPUT_KEY_UNKNOWN },
    /* 0x3e */ { INPUT_KEY_F4,                  INPUT_KEY_UNKNOWN },
    /* 0x3f */ { INPUT_KEY_F5,                  INPUT_KEY_UNKNOWN },
    /* 0x40 */ { INPUT_KEY_F6,                  INPUT_KEY_UNKNOWN },
    /* 0x41 */ { INPUT_KEY_F7,                  INPUT_KEY_UNKNOWN },
    /* 0x42 */ { INPUT_KEY_F8,                  INPUT_KEY_UNKNOWN },
    /* 0x43 */ { INPUT_KEY_F9,                  INPUT_KEY_UNKNOWN },
    /* 0x44 */ { INPUT_KEY_F10,                 INPUT_KEY_UNKNOWN },
    /* 0x45 */ { INPUT_KEY_NUM_LOCK,            INPUT_KEY_UNKNOWN },
    /* 0x46 */ { INPUT_KEY_SCROLL_LOCK,         INPUT_KEY_UNKNOWN },
    /* 0x47 */ { INPUT_KEY_KP_7,                INPUT_KEY_HOME },
    /* 0x48 */ { INPUT_KEY_KP_8,                INPUT_KEY_UP },
    /* 0x49 */ { INPUT_KEY_KP_9,                INPUT_KEY_PAGE_UP },
    /* 0x4a */ { INPUT_KEY_KP_MINUS,            INPUT_KEY_UNKNOWN },
    /* 0x4b */ { INPUT_KEY_KP_4,                INPUT_KEY_LEFT },
    /* 0x4c */ { INPUT_KEY_KP_5,                INPUT_KEY_UNKNOWN },
    /* 0x4d */ { INPUT_KEY_KP_6,                INPUT_KEY_RIGHT },
    /* 0x4e */ { INPUT_KEY_KP_PLUS,             INPUT_KEY_UNKNOWN },
    /* 0x4f */ { INPUT_KEY_KP_1,                INPUT_KEY_END },
    /* 0x50 */ { INPUT_KEY_KP_2,                INPUT_KEY_DOWN },
    /* 0x51 */ { INPUT_KEY_KP_3,                INPUT_KEY_PAGE_DOWN },
    /* 0x52 */ { INPUT_KEY_KP_0,                INPUT_KEY_INSERT },
    /* 0x53 */ { INPUT_KEY_KP_PERIOD,           INPUT_KEY_DELETE },
    /* 0x54 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x55 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x56 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x57 */ { INPUT_KEY_F11,                 INPUT_KEY_UNKNOWN },
    /* 0x58 */ { INPUT_KEY_F12,                 INPUT_KEY_UNKNOWN },
    /* 0x59 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x5a */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x5b */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_LEFT_SUPER },
    /* 0x5c */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x5d */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_RIGHT_SUPER },
    /* 0x5e */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x5f */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x60 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x61 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x62 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x63 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x64 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x65 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x66 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x67 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x68 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x69 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6a */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6b */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6c */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6d */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6e */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x6f */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x70 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x71 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x72 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x73 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x74 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x75 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x76 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x77 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x78 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x79 */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7a */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7b */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7c */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7d */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7e */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
    /* 0x7f */ { INPUT_KEY_UNKNOWN,             INPUT_KEY_UNKNOWN },
};
