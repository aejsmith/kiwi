/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Input device class interface.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** Input device class name. */
#define INPUT_DEVICE_CLASS_NAME     "input"

/** Input device class attribute names. */
#define INPUT_DEVICE_ATTR_TYPE      "input.type"

/** INPUT_DEVICE_ATTR_TYPE - Device type (int32). */
typedef enum input_device_type {
    INPUT_DEVICE_KEYBOARD           = 0,    /**< Keyboard. */
    INPUT_DEVICE_MOUSE              = 1,    /**< Mouse. */
} input_device_type_t;

/** Input event types. */
typedef enum input_event_type {
    INPUT_EVENT_KEY_DOWN            = 0,    /**< Key down (keyboard). */
    INPUT_EVENT_KEY_UP              = 1,    /**< Key up (keyboard). */
    INPUT_EVENT_REL_X               = 2,    /**< Relative X movement (mouse). */
    INPUT_EVENT_REL_Y               = 3,    /**< Relative Y movement (mouse). */
    INPUT_EVENT_BUTTON_DOWN         = 4,    /**< Button down (mouse). */
    INPUT_EVENT_BUTTON_UP           = 5,    /**< Button up (mouse). */
} input_event_type_t;

/** Input event information structure. */
typedef struct input_event {
    nstime_t time;                          /**< Time since boot that event occurred at. */
    input_event_type_t type;                /**< Event type. */
    int32_t value;                          /**< Value specific to the event type. */
} input_event_t;

#define INPUT_KEY(page, code)   ((page << 16) | code)

/**
 * Keyboard key codes (INPUT_EVENT_KEY_*)
 *
 * These codes are all based on the USB HID Usage Tables specification 1.21:
 * https://usb.org/document-library/hid-usage-tables-121
 *
 * USB usage page is in bits 16-23, usage ID is in bits 0-15.
 */
enum {
    INPUT_KEY_UNKNOWN = 0,

    /**
     * Usage page 0x07 (Keyboard/Keypad Page).
     */

    INPUT_KEY_A                     = INPUT_KEY(0x07, 0x4),
    INPUT_KEY_B                     = INPUT_KEY(0x07, 0x5),
    INPUT_KEY_C                     = INPUT_KEY(0x07, 0x6),
    INPUT_KEY_D                     = INPUT_KEY(0x07, 0x7),
    INPUT_KEY_E                     = INPUT_KEY(0x07, 0x8),
    INPUT_KEY_F                     = INPUT_KEY(0x07, 0x9),
    INPUT_KEY_G                     = INPUT_KEY(0x07, 0xa),
    INPUT_KEY_H                     = INPUT_KEY(0x07, 0xb),
    INPUT_KEY_I                     = INPUT_KEY(0x07, 0xc),
    INPUT_KEY_J                     = INPUT_KEY(0x07, 0xd),
    INPUT_KEY_K                     = INPUT_KEY(0x07, 0xe),
    INPUT_KEY_L                     = INPUT_KEY(0x07, 0xf),
    INPUT_KEY_M                     = INPUT_KEY(0x07, 0x10),
    INPUT_KEY_N                     = INPUT_KEY(0x07, 0x11),
    INPUT_KEY_O                     = INPUT_KEY(0x07, 0x12),
    INPUT_KEY_P                     = INPUT_KEY(0x07, 0x13),
    INPUT_KEY_Q                     = INPUT_KEY(0x07, 0x14),
    INPUT_KEY_R                     = INPUT_KEY(0x07, 0x15),
    INPUT_KEY_S                     = INPUT_KEY(0x07, 0x16),
    INPUT_KEY_T                     = INPUT_KEY(0x07, 0x17),
    INPUT_KEY_U                     = INPUT_KEY(0x07, 0x18),
    INPUT_KEY_V                     = INPUT_KEY(0x07, 0x19),
    INPUT_KEY_W                     = INPUT_KEY(0x07, 0x1a),
    INPUT_KEY_X                     = INPUT_KEY(0x07, 0x1b),
    INPUT_KEY_Y                     = INPUT_KEY(0x07, 0x1c),
    INPUT_KEY_Z                     = INPUT_KEY(0x07, 0x1d),
    INPUT_KEY_1                     = INPUT_KEY(0x07, 0x1e),
    INPUT_KEY_2                     = INPUT_KEY(0x07, 0x1f),
    INPUT_KEY_3                     = INPUT_KEY(0x07, 0x20),
    INPUT_KEY_4                     = INPUT_KEY(0x07, 0x21),
    INPUT_KEY_5                     = INPUT_KEY(0x07, 0x22),
    INPUT_KEY_6                     = INPUT_KEY(0x07, 0x23),
    INPUT_KEY_7                     = INPUT_KEY(0x07, 0x24),
    INPUT_KEY_8                     = INPUT_KEY(0x07, 0x25),
    INPUT_KEY_9                     = INPUT_KEY(0x07, 0x26),
    INPUT_KEY_0                     = INPUT_KEY(0x07, 0x27),
    INPUT_KEY_RETURN                = INPUT_KEY(0x07, 0x28),
    INPUT_KEY_ESCAPE                = INPUT_KEY(0x07, 0x29),
    INPUT_KEY_BACKSPACE             = INPUT_KEY(0x07, 0x2a),
    INPUT_KEY_TAB                   = INPUT_KEY(0x07, 0x2b),
    INPUT_KEY_SPACE                 = INPUT_KEY(0x07, 0x2c),
    INPUT_KEY_MINUS                 = INPUT_KEY(0x07, 0x2d),
    INPUT_KEY_EQUALS                = INPUT_KEY(0x07, 0x2e),
    INPUT_KEY_LEFT_BRACKET          = INPUT_KEY(0x07, 0x2f),
    INPUT_KEY_RIGHT_BRACKET         = INPUT_KEY(0x07, 0x30),
    INPUT_KEY_BACKSLASH             = INPUT_KEY(0x07, 0x31),
    INPUT_KEY_NONUS_HASH            = INPUT_KEY(0x07, 0x32),
    INPUT_KEY_SEMICOLON             = INPUT_KEY(0x07, 0x33),
    INPUT_KEY_APOSTROPHE            = INPUT_KEY(0x07, 0x34),
    INPUT_KEY_GRAVE                 = INPUT_KEY(0x07, 0x35),
    INPUT_KEY_COMMA                 = INPUT_KEY(0x07, 0x36),
    INPUT_KEY_PERIOD                = INPUT_KEY(0x07, 0x37),
    INPUT_KEY_SLASH                 = INPUT_KEY(0x07, 0x38),
    INPUT_KEY_CAPS_LOCK             = INPUT_KEY(0x07, 0x39),
    INPUT_KEY_F1                    = INPUT_KEY(0x07, 0x3a),
    INPUT_KEY_F2                    = INPUT_KEY(0x07, 0x3b),
    INPUT_KEY_F3                    = INPUT_KEY(0x07, 0x3c),
    INPUT_KEY_F4                    = INPUT_KEY(0x07, 0x3d),
    INPUT_KEY_F5                    = INPUT_KEY(0x07, 0x3e),
    INPUT_KEY_F6                    = INPUT_KEY(0x07, 0x3f),
    INPUT_KEY_F7                    = INPUT_KEY(0x07, 0x40),
    INPUT_KEY_F8                    = INPUT_KEY(0x07, 0x41),
    INPUT_KEY_F9                    = INPUT_KEY(0x07, 0x42),
    INPUT_KEY_F10                   = INPUT_KEY(0x07, 0x43),
    INPUT_KEY_F11                   = INPUT_KEY(0x07, 0x44),
    INPUT_KEY_F12                   = INPUT_KEY(0x07, 0x45),
    INPUT_KEY_PRINT_SCREEN          = INPUT_KEY(0x07, 0x46),
    INPUT_KEY_SCROLL_LOCK           = INPUT_KEY(0x07, 0x47),
    INPUT_KEY_PAUSE                 = INPUT_KEY(0x07, 0x48),
    INPUT_KEY_INSERT                = INPUT_KEY(0x07, 0x49),
    INPUT_KEY_HOME                  = INPUT_KEY(0x07, 0x4a),
    INPUT_KEY_PAGE_UP               = INPUT_KEY(0x07, 0x4b),
    INPUT_KEY_DELETE                = INPUT_KEY(0x07, 0x4c),
    INPUT_KEY_END                   = INPUT_KEY(0x07, 0x4d),
    INPUT_KEY_PAGE_DOWN             = INPUT_KEY(0x07, 0x4e),
    INPUT_KEY_RIGHT                 = INPUT_KEY(0x07, 0x4f),
    INPUT_KEY_LEFT                  = INPUT_KEY(0x07, 0x50),
    INPUT_KEY_DOWN                  = INPUT_KEY(0x07, 0x51),
    INPUT_KEY_UP                    = INPUT_KEY(0x07, 0x52),
    INPUT_KEY_NUM_LOCK              = INPUT_KEY(0x07, 0x53),
    INPUT_KEY_KP_DIVIDE             = INPUT_KEY(0x07, 0x54),
    INPUT_KEY_KP_ASTERISK           = INPUT_KEY(0x07, 0x55),
    INPUT_KEY_KP_MINUS              = INPUT_KEY(0x07, 0x56),
    INPUT_KEY_KP_PLUS               = INPUT_KEY(0x07, 0x57),
    INPUT_KEY_KP_ENTER              = INPUT_KEY(0x07, 0x58),
    INPUT_KEY_KP_1                  = INPUT_KEY(0x07, 0x59),
    INPUT_KEY_KP_2                  = INPUT_KEY(0x07, 0x5a),
    INPUT_KEY_KP_3                  = INPUT_KEY(0x07, 0x5b),
    INPUT_KEY_KP_4                  = INPUT_KEY(0x07, 0x5c),
    INPUT_KEY_KP_5                  = INPUT_KEY(0x07, 0x5d),
    INPUT_KEY_KP_6                  = INPUT_KEY(0x07, 0x5e),
    INPUT_KEY_KP_7                  = INPUT_KEY(0x07, 0x5f),
    INPUT_KEY_KP_8                  = INPUT_KEY(0x07, 0x60),
    INPUT_KEY_KP_9                  = INPUT_KEY(0x07, 0x61),
    INPUT_KEY_KP_0                  = INPUT_KEY(0x07, 0x62),
    INPUT_KEY_KP_PERIOD             = INPUT_KEY(0x07, 0x63),
    INPUT_KEY_NONUS_BACKSLASH       = INPUT_KEY(0x07, 0x64),
    INPUT_KEY_APPLICATION           = INPUT_KEY(0x07, 0x65),
    INPUT_KEY_POWER                 = INPUT_KEY(0x07, 0x66),
    INPUT_KEY_KP_EQUALS             = INPUT_KEY(0x07, 0x67),
    INPUT_KEY_F13                   = INPUT_KEY(0x07, 0x68),
    INPUT_KEY_F14                   = INPUT_KEY(0x07, 0x69),
    INPUT_KEY_F15                   = INPUT_KEY(0x07, 0x6a),
    INPUT_KEY_F16                   = INPUT_KEY(0x07, 0x6b),
    INPUT_KEY_F17                   = INPUT_KEY(0x07, 0x6c),
    INPUT_KEY_F18                   = INPUT_KEY(0x07, 0x6d),
    INPUT_KEY_F19                   = INPUT_KEY(0x07, 0x6e),
    INPUT_KEY_F20                   = INPUT_KEY(0x07, 0x6f),
    INPUT_KEY_F21                   = INPUT_KEY(0x07, 0x70),
    INPUT_KEY_F22                   = INPUT_KEY(0x07, 0x71),
    INPUT_KEY_F23                   = INPUT_KEY(0x07, 0x72),
    INPUT_KEY_F24                   = INPUT_KEY(0x07, 0x73),
    INPUT_KEY_EXECUTE               = INPUT_KEY(0x07, 0x74),
    INPUT_KEY_HELP                  = INPUT_KEY(0x07, 0x75),
    INPUT_KEY_MENU                  = INPUT_KEY(0x07, 0x76),
    INPUT_KEY_SELECT                = INPUT_KEY(0x07, 0x77),
    INPUT_KEY_STOP                  = INPUT_KEY(0x07, 0x78),
    INPUT_KEY_AGAIN                 = INPUT_KEY(0x07, 0x79),
    INPUT_KEY_UNDO                  = INPUT_KEY(0x07, 0x7a),
    INPUT_KEY_CUT                   = INPUT_KEY(0x07, 0x7b),
    INPUT_KEY_COPY                  = INPUT_KEY(0x07, 0x7c),
    INPUT_KEY_PASTE                 = INPUT_KEY(0x07, 0x7d),
    INPUT_KEY_FIND                  = INPUT_KEY(0x07, 0x7e),
    INPUT_KEY_MUTE                  = INPUT_KEY(0x07, 0x7f),
    INPUT_KEY_VOLUME_UP             = INPUT_KEY(0x07, 0x80),
    INPUT_KEY_VOLUME_DOWN           = INPUT_KEY(0x07, 0x81),
    /* Locking Caps/Num/Scroll Lock omitted. */
    INPUT_KEY_KP_COMMA              = INPUT_KEY(0x07, 0x85),
    INPUT_KEY_KP_EQUALS_AS400       = INPUT_KEY(0x07, 0x86),
    INPUT_KEY_INTERNATIONAL1        = INPUT_KEY(0x07, 0x87),
    INPUT_KEY_INTERNATIONAL2        = INPUT_KEY(0x07, 0x88),
    INPUT_KEY_INTERNATIONAL3        = INPUT_KEY(0x07, 0x89),
    INPUT_KEY_INTERNATIONAL4        = INPUT_KEY(0x07, 0x8a),
    INPUT_KEY_INTERNATIONAL5        = INPUT_KEY(0x07, 0x8b),
    INPUT_KEY_INTERNATIONAL6        = INPUT_KEY(0x07, 0x8c),
    INPUT_KEY_INTERNATIONAL7        = INPUT_KEY(0x07, 0x8d),
    INPUT_KEY_INTERNATIONAL8        = INPUT_KEY(0x07, 0x8e),
    INPUT_KEY_INTERNATIONAL9        = INPUT_KEY(0x07, 0x8f),
    INPUT_KEY_LANG1                 = INPUT_KEY(0x07, 0x90),
    INPUT_KEY_LANG2                 = INPUT_KEY(0x07, 0x91),
    INPUT_KEY_LANG3                 = INPUT_KEY(0x07, 0x92),
    INPUT_KEY_LANG4                 = INPUT_KEY(0x07, 0x93),
    INPUT_KEY_LANG5                 = INPUT_KEY(0x07, 0x94),
    INPUT_KEY_LANG6                 = INPUT_KEY(0x07, 0x95),
    INPUT_KEY_LANG7                 = INPUT_KEY(0x07, 0x96),
    INPUT_KEY_LANG8                 = INPUT_KEY(0x07, 0x97),
    INPUT_KEY_LANG9                 = INPUT_KEY(0x07, 0x98),
    INPUT_KEY_ALT_ERASE             = INPUT_KEY(0x07, 0x99),
    INPUT_KEY_SYSREQ                = INPUT_KEY(0x07, 0x9a),
    INPUT_KEY_CANCEL                = INPUT_KEY(0x07, 0x9b),
    INPUT_KEY_CLEAR                 = INPUT_KEY(0x07, 0x9c),
    INPUT_KEY_PRIOR                 = INPUT_KEY(0x07, 0x9d),
    INPUT_KEY_RETURN2               = INPUT_KEY(0x07, 0x9e),
    INPUT_KEY_SEPARATOR             = INPUT_KEY(0x07, 0x9f),
    INPUT_KEY_OUT                   = INPUT_KEY(0x07, 0xa0),
    INPUT_KEY_OPER                  = INPUT_KEY(0x07, 0xa1),
    INPUT_KEY_CLEAR_AGAIN           = INPUT_KEY(0x07, 0xa2),
    INPUT_KEY_CRSEL                 = INPUT_KEY(0x07, 0xa3),
    INPUT_KEY_EXSEL                 = INPUT_KEY(0x07, 0xa4),
    /* Reserved. */
    INPUT_KEY_KP_00                 = INPUT_KEY(0x07, 0xb0),
    INPUT_KEY_KP_000                = INPUT_KEY(0x07, 0xb1),
    INPUT_KEY_THOUSANDS_SEPARATOR   = INPUT_KEY(0x07, 0xb2),
    INPUT_KEY_DECIMAL_SEPARATOR     = INPUT_KEY(0x07, 0xb3),
    INPUT_KEY_CURRENCY_UNIT         = INPUT_KEY(0x07, 0xb4),
    INPUT_KEY_CURRENCY_SUBUNIT      = INPUT_KEY(0x07, 0xb5),
    INPUT_KEY_KP_LEFT_PAREN         = INPUT_KEY(0x07, 0xb6),
    INPUT_KEY_KP_RIGHT_PAREN        = INPUT_KEY(0x07, 0xb7),
    INPUT_KEY_KP_LEFT_BRACE         = INPUT_KEY(0x07, 0xb8),
    INPUT_KEY_KP_RIGHT_BRACE        = INPUT_KEY(0x07, 0xb9),
    INPUT_KEY_KP_TAB                = INPUT_KEY(0x07, 0xba),
    INPUT_KEY_KP_BACKSPACE          = INPUT_KEY(0x07, 0xbb),
    INPUT_KEY_KP_A                  = INPUT_KEY(0x07, 0xbc),
    INPUT_KEY_KP_B                  = INPUT_KEY(0x07, 0xbd),
    INPUT_KEY_KP_C                  = INPUT_KEY(0x07, 0xbe),
    INPUT_KEY_KP_D                  = INPUT_KEY(0x07, 0xbf),
    INPUT_KEY_KP_E                  = INPUT_KEY(0x07, 0xc0),
    INPUT_KEY_KP_F                  = INPUT_KEY(0x07, 0xc1),
    INPUT_KEY_KP_XOR                = INPUT_KEY(0x07, 0xc2),
    INPUT_KEY_KP_POWER              = INPUT_KEY(0x07, 0xc3),
    INPUT_KEY_KP_PERCENT            = INPUT_KEY(0x07, 0xc4),
    INPUT_KEY_KP_LESS               = INPUT_KEY(0x07, 0xc5),
    INPUT_KEY_KP_GREATER            = INPUT_KEY(0x07, 0xc6),
    INPUT_KEY_KP_AMPERSAND          = INPUT_KEY(0x07, 0xc7),
    INPUT_KEY_KP_DBL_AMPERSAND      = INPUT_KEY(0x07, 0xc8),
    INPUT_KEY_KP_VERTICAL_BAR       = INPUT_KEY(0x07, 0xc9),
    INPUT_KEY_KP_DBL_VERTICAL_BAR   = INPUT_KEY(0x07, 0xca),
    INPUT_KEY_KP_COLON              = INPUT_KEY(0x07, 0xcb),
    INPUT_KEY_KP_HASH               = INPUT_KEY(0x07, 0xcc),
    INPUT_KEY_KP_SPACE              = INPUT_KEY(0x07, 0xcd),
    INPUT_KEY_KP_AT                 = INPUT_KEY(0x07, 0xce),
    INPUT_KEY_KP_EXCLAM             = INPUT_KEY(0x07, 0xcf),
    INPUT_KEY_KP_MEM_STORE          = INPUT_KEY(0x07, 0xd0),
    INPUT_KEY_KP_MEM_RECALL         = INPUT_KEY(0x07, 0xd1),
    INPUT_KEY_KP_MEM_CLEAR          = INPUT_KEY(0x07, 0xd2),
    INPUT_KEY_KP_MEM_ADD            = INPUT_KEY(0x07, 0xd3),
    INPUT_KEY_KP_MEM_SUBTRACT       = INPUT_KEY(0x07, 0xd4),
    INPUT_KEY_KP_MEM_MULTIPLY       = INPUT_KEY(0x07, 0xd5),
    INPUT_KEY_KP_MEM_DIVIDE         = INPUT_KEY(0x07, 0xd6),
    INPUT_KEY_KP_PLUS_MINUS         = INPUT_KEY(0x07, 0xd7),
    INPUT_KEY_KP_CLEAR              = INPUT_KEY(0x07, 0xd8),
    INPUT_KEY_KP_CLEAR_ENTRY        = INPUT_KEY(0x07, 0xd9),
    INPUT_KEY_KP_BINARY             = INPUT_KEY(0x07, 0xda),
    INPUT_KEY_KP_OCTAL              = INPUT_KEY(0x07, 0xdb),
    INPUT_KEY_KP_DECIMAL            = INPUT_KEY(0x07, 0xdc),
    INPUT_KEY_KP_HEXADECIMAL        = INPUT_KEY(0x07, 0xdd),
    /* Reserved. */
    INPUT_KEY_LEFT_CTRL             = INPUT_KEY(0x07, 0xe0),
    INPUT_KEY_LEFT_SHIFT            = INPUT_KEY(0x07, 0xe1),
    INPUT_KEY_LEFT_ALT              = INPUT_KEY(0x07, 0xe2),
    INPUT_KEY_LEFT_SUPER            = INPUT_KEY(0x07, 0xe3),
    INPUT_KEY_RIGHT_CTRL            = INPUT_KEY(0x07, 0xe4),
    INPUT_KEY_RIGHT_SHIFT           = INPUT_KEY(0x07, 0xe5),
    INPUT_KEY_RIGHT_ALT             = INPUT_KEY(0x07, 0xe6),
    INPUT_KEY_RIGHT_SUPER           = INPUT_KEY(0x07, 0xe7),
};

#undef INPUT_KEY

/** Mouse buttons (INPUT_EVENT_BUTTON_*). */
enum {
    INPUT_BUTTON_LEFT               = 0,    /**< Left Button. */
    INPUT_BUTTON_RIGHT              = 1,    /**< Right Button. */
    INPUT_BUTTON_MIDDLE             = 2,    /**< Middle Button. */
};

__KERNEL_EXTERN_C_END
