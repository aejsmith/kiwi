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
 * @brief               Keyboard mapping table.
 */

#include <device/input.h>

/** Map an uppercase ASCII character to a control character. */
#define to_control(c)   ((c) & 0x1f)

/** Get the index within the keyboard usage page. */
#define index(key)      ((key) & 0xff)

/** Normal key table. */
uint8_t g_keyTable[256] = {
    [index(INPUT_KEY_LEFT_CTRL)]     = 0,
    [index(INPUT_KEY_LEFT_ALT)]      = 0,
    [index(INPUT_KEY_LEFT_SUPER)]    = 0,
    [index(INPUT_KEY_LEFT_SHIFT)]    = 0,
    [index(INPUT_KEY_RIGHT_CTRL)]    = 0,
    [index(INPUT_KEY_RIGHT_ALT)]     = 0,
    [index(INPUT_KEY_RIGHT_SUPER)]   = 0,
    [index(INPUT_KEY_RIGHT_SHIFT)]   = 0,
    [index(INPUT_KEY_CAPS_LOCK)]     = 0,
    [index(INPUT_KEY_SCROLL_LOCK)]   = 0,
    [index(INPUT_KEY_NUM_LOCK)]      = 0,
    [index(INPUT_KEY_ESCAPE)]        = to_control('['),
    [index(INPUT_KEY_F1)]            = 0,
    [index(INPUT_KEY_F2)]            = 0,
    [index(INPUT_KEY_F3)]            = 0,
    [index(INPUT_KEY_F4)]            = 0,
    [index(INPUT_KEY_F5)]            = 0,
    [index(INPUT_KEY_F6)]            = 0,
    [index(INPUT_KEY_F7)]            = 0,
    [index(INPUT_KEY_F8)]            = 0,
    [index(INPUT_KEY_F9)]            = 0,
    [index(INPUT_KEY_F10)]           = 0,
    [index(INPUT_KEY_F11)]           = 0,
    [index(INPUT_KEY_F12)]           = 0,
    [index(INPUT_KEY_PRINT_SCREEN)]  = 0,
    [index(INPUT_KEY_PAUSE)]         = 0,
    [index(INPUT_KEY_0)]             = '0',
    [index(INPUT_KEY_1)]             = '1',
    [index(INPUT_KEY_2)]             = '2',
    [index(INPUT_KEY_3)]             = '3',
    [index(INPUT_KEY_4)]             = '4',
    [index(INPUT_KEY_5)]             = '5',
    [index(INPUT_KEY_6)]             = '6',
    [index(INPUT_KEY_7)]             = '7',
    [index(INPUT_KEY_8)]             = '8',
    [index(INPUT_KEY_9)]             = '9',
    [index(INPUT_KEY_MINUS)]         = '-',
    [index(INPUT_KEY_EQUALS)]        = '=',
    [index(INPUT_KEY_BACKSPACE)]     = '\b',
    [index(INPUT_KEY_TAB)]           = '\t',
    [index(INPUT_KEY_Q)]             = 'q',
    [index(INPUT_KEY_W)]             = 'w',
    [index(INPUT_KEY_E)]             = 'e',
    [index(INPUT_KEY_R)]             = 'r',
    [index(INPUT_KEY_T)]             = 't',
    [index(INPUT_KEY_Y)]             = 'y',
    [index(INPUT_KEY_U)]             = 'u',
    [index(INPUT_KEY_I)]             = 'i',
    [index(INPUT_KEY_O)]             = 'o',
    [index(INPUT_KEY_P)]             = 'p',
    [index(INPUT_KEY_LEFT_BRACKET)]  = '[',
    [index(INPUT_KEY_RIGHT_BRACKET)] = ']',
    [index(INPUT_KEY_RETURN)]        = '\r',
    [index(INPUT_KEY_A)]             = 'a',
    [index(INPUT_KEY_S)]             = 's',
    [index(INPUT_KEY_D)]             = 'd',
    [index(INPUT_KEY_F)]             = 'f',
    [index(INPUT_KEY_G)]             = 'g',
    [index(INPUT_KEY_H)]             = 'h',
    [index(INPUT_KEY_J)]             = 'j',
    [index(INPUT_KEY_K)]             = 'k',
    [index(INPUT_KEY_L)]             = 'l',
    [index(INPUT_KEY_SEMICOLON)]     = ';',
    [index(INPUT_KEY_APOSTROPHE)]    = '\'',
    [index(INPUT_KEY_BACKSLASH)]     = '\\',
    [index(INPUT_KEY_GRAVE)]         = '`',
    [index(INPUT_KEY_Z)]             = 'z',
    [index(INPUT_KEY_X)]             = 'x',
    [index(INPUT_KEY_C)]             = 'c',
    [index(INPUT_KEY_V)]             = 'v',
    [index(INPUT_KEY_B)]             = 'b',
    [index(INPUT_KEY_N)]             = 'n',
    [index(INPUT_KEY_M)]             = 'm',
    [index(INPUT_KEY_COMMA)]         = ',',
    [index(INPUT_KEY_PERIOD)]        = '.',
    [index(INPUT_KEY_SLASH)]         = '/',
    [index(INPUT_KEY_SPACE)]         = ' ',
    [index(INPUT_KEY_LEFT)]          = 0,
    [index(INPUT_KEY_RIGHT)]         = 0,
    [index(INPUT_KEY_UP)]            = 0,
    [index(INPUT_KEY_DOWN)]          = 0,
    [index(INPUT_KEY_INSERT)]        = 0,
    [index(INPUT_KEY_DELETE)]        = 0x7f,
    [index(INPUT_KEY_HOME)]          = 0,
    [index(INPUT_KEY_END)]           = 0,
    [index(INPUT_KEY_PAGE_UP)]       = 0,
    [index(INPUT_KEY_PAGE_DOWN)]     = 0,
    [index(INPUT_KEY_KP_DIVIDE)]     = '/',
    [index(INPUT_KEY_KP_ASTERISK)]   = '*',
    [index(INPUT_KEY_KP_MINUS)]      = '-',
    [index(INPUT_KEY_KP_PLUS)]       = '+',
    [index(INPUT_KEY_KP_ENTER)]      = '\r',
    [index(INPUT_KEY_KP_7)]          = '7',
    [index(INPUT_KEY_KP_8)]          = '8',
    [index(INPUT_KEY_KP_9)]          = '9',
    [index(INPUT_KEY_KP_4)]          = '4',
    [index(INPUT_KEY_KP_5)]          = '5',
    [index(INPUT_KEY_KP_6)]          = '6',
    [index(INPUT_KEY_KP_1)]          = '7',
    [index(INPUT_KEY_KP_2)]          = '8',
    [index(INPUT_KEY_KP_3)]          = '9',
    [index(INPUT_KEY_KP_0)]          = '0',
    [index(INPUT_KEY_KP_PERIOD)]     = '.',
};

/** Key table with Shift enabled. */
uint8_t g_keyTableShift[256] = {
    [index(INPUT_KEY_LEFT_CTRL)]     = 0,
    [index(INPUT_KEY_LEFT_ALT)]      = 0,
    [index(INPUT_KEY_LEFT_SUPER)]    = 0,
    [index(INPUT_KEY_LEFT_SHIFT)]    = 0,
    [index(INPUT_KEY_RIGHT_CTRL)]    = 0,
    [index(INPUT_KEY_RIGHT_ALT)]     = 0,
    [index(INPUT_KEY_RIGHT_SUPER)]   = 0,
    [index(INPUT_KEY_RIGHT_SHIFT)]   = 0,
    [index(INPUT_KEY_CAPS_LOCK)]     = 0,
    [index(INPUT_KEY_SCROLL_LOCK)]   = 0,
    [index(INPUT_KEY_NUM_LOCK)]      = 0,
    [index(INPUT_KEY_ESCAPE)]        = to_control('['),
    [index(INPUT_KEY_F1)]            = 0,
    [index(INPUT_KEY_F2)]            = 0,
    [index(INPUT_KEY_F3)]            = 0,
    [index(INPUT_KEY_F4)]            = 0,
    [index(INPUT_KEY_F5)]            = 0,
    [index(INPUT_KEY_F6)]            = 0,
    [index(INPUT_KEY_F7)]            = 0,
    [index(INPUT_KEY_F8)]            = 0,
    [index(INPUT_KEY_F9)]            = 0,
    [index(INPUT_KEY_F10)]           = 0,
    [index(INPUT_KEY_F11)]           = 0,
    [index(INPUT_KEY_F12)]           = 0,
    [index(INPUT_KEY_PRINT_SCREEN)]  = 0,
    [index(INPUT_KEY_PAUSE)]         = 0,
    [index(INPUT_KEY_0)]             = ')',
    [index(INPUT_KEY_1)]             = '!',
    [index(INPUT_KEY_2)]             = '@',
    [index(INPUT_KEY_3)]             = '#',
    [index(INPUT_KEY_4)]             = '$',
    [index(INPUT_KEY_5)]             = '%',
    [index(INPUT_KEY_6)]             = '^',
    [index(INPUT_KEY_7)]             = '&',
    [index(INPUT_KEY_8)]             = '*',
    [index(INPUT_KEY_9)]             = '(',
    [index(INPUT_KEY_MINUS)]         = '_',
    [index(INPUT_KEY_EQUALS)]        = '+',
    [index(INPUT_KEY_BACKSPACE)]     = '\b',
    [index(INPUT_KEY_TAB)]           = '\t',
    [index(INPUT_KEY_Q)]             = 'Q',
    [index(INPUT_KEY_W)]             = 'W',
    [index(INPUT_KEY_E)]             = 'E',
    [index(INPUT_KEY_R)]             = 'R',
    [index(INPUT_KEY_T)]             = 'T',
    [index(INPUT_KEY_Y)]             = 'Y',
    [index(INPUT_KEY_U)]             = 'U',
    [index(INPUT_KEY_I)]             = 'I',
    [index(INPUT_KEY_O)]             = 'O',
    [index(INPUT_KEY_P)]             = 'P',
    [index(INPUT_KEY_LEFT_BRACKET)]  = '{',
    [index(INPUT_KEY_RIGHT_BRACKET)] = '}',
    [index(INPUT_KEY_RETURN)]        = '\n',
    [index(INPUT_KEY_A)]             = 'A',
    [index(INPUT_KEY_S)]             = 'S',
    [index(INPUT_KEY_D)]             = 'D',
    [index(INPUT_KEY_F)]             = 'F',
    [index(INPUT_KEY_G)]             = 'G',
    [index(INPUT_KEY_H)]             = 'H',
    [index(INPUT_KEY_J)]             = 'J',
    [index(INPUT_KEY_K)]             = 'K',
    [index(INPUT_KEY_L)]             = 'L',
    [index(INPUT_KEY_SEMICOLON)]     = ':',
    [index(INPUT_KEY_APOSTROPHE)]    = '"',
    [index(INPUT_KEY_BACKSLASH)]     = '|',
    [index(INPUT_KEY_GRAVE)]         = '~',
    [index(INPUT_KEY_Z)]             = 'Z',
    [index(INPUT_KEY_X)]             = 'X',
    [index(INPUT_KEY_C)]             = 'C',
    [index(INPUT_KEY_V)]             = 'V',
    [index(INPUT_KEY_B)]             = 'B',
    [index(INPUT_KEY_N)]             = 'N',
    [index(INPUT_KEY_M)]             = 'M',
    [index(INPUT_KEY_COMMA)]         = '<',
    [index(INPUT_KEY_PERIOD)]        = '>',
    [index(INPUT_KEY_SLASH)]         = '?',
    [index(INPUT_KEY_SPACE)]         = ' ',
    [index(INPUT_KEY_LEFT)]          = 0,
    [index(INPUT_KEY_RIGHT)]         = 0,
    [index(INPUT_KEY_UP)]            = 0,
    [index(INPUT_KEY_DOWN)]          = 0,
    [index(INPUT_KEY_INSERT)]        = 0,
    [index(INPUT_KEY_DELETE)]        = 0,
    [index(INPUT_KEY_HOME)]          = 0,
    [index(INPUT_KEY_END)]           = 0,
    [index(INPUT_KEY_PAGE_UP)]       = 0,
    [index(INPUT_KEY_PAGE_DOWN)]     = 0,
    [index(INPUT_KEY_KP_DIVIDE)]     = '/',
    [index(INPUT_KEY_KP_ASTERISK)]   = '*',
    [index(INPUT_KEY_KP_MINUS)]      = '-',
    [index(INPUT_KEY_KP_PLUS)]       = '+',
    [index(INPUT_KEY_KP_ENTER)]      = '\n',
    [index(INPUT_KEY_KP_7)]          = 0,
    [index(INPUT_KEY_KP_8)]          = 0,
    [index(INPUT_KEY_KP_9)]          = 0,
    [index(INPUT_KEY_KP_4)]          = 0,
    [index(INPUT_KEY_KP_5)]          = 0,
    [index(INPUT_KEY_KP_6)]          = 0,
    [index(INPUT_KEY_KP_1)]          = 0,
    [index(INPUT_KEY_KP_2)]          = 0,
    [index(INPUT_KEY_KP_3)]          = 0,
    [index(INPUT_KEY_KP_0)]          = 0,
    [index(INPUT_KEY_KP_PERIOD)]     = 0,
};

/**
 * Key table with Ctrl pressed. If an entry is 0, the entry from the
 * normal/shift table is used instead.
 */
uint8_t g_keyTableCtrl[256] = {
    [index(INPUT_KEY_LEFT_CTRL)]     = 0,
    [index(INPUT_KEY_LEFT_ALT)]      = 0,
    [index(INPUT_KEY_LEFT_SUPER)]    = 0,
    [index(INPUT_KEY_LEFT_SHIFT)]    = 0,
    [index(INPUT_KEY_RIGHT_CTRL)]    = 0,
    [index(INPUT_KEY_RIGHT_ALT)]     = 0,
    [index(INPUT_KEY_RIGHT_SUPER)]   = 0,
    [index(INPUT_KEY_RIGHT_SHIFT)]   = 0,
    [index(INPUT_KEY_CAPS_LOCK)]     = 0,
    [index(INPUT_KEY_SCROLL_LOCK)]   = 0,
    [index(INPUT_KEY_NUM_LOCK)]      = 0,
    [index(INPUT_KEY_ESCAPE)]        = 0,
    [index(INPUT_KEY_F1)]            = 0,
    [index(INPUT_KEY_F2)]            = 0,
    [index(INPUT_KEY_F3)]            = 0,
    [index(INPUT_KEY_F4)]            = 0,
    [index(INPUT_KEY_F5)]            = 0,
    [index(INPUT_KEY_F6)]            = 0,
    [index(INPUT_KEY_F7)]            = 0,
    [index(INPUT_KEY_F8)]            = 0,
    [index(INPUT_KEY_F9)]            = 0,
    [index(INPUT_KEY_F10)]           = 0,
    [index(INPUT_KEY_F11)]           = 0,
    [index(INPUT_KEY_F12)]           = 0,
    [index(INPUT_KEY_PRINT_SCREEN)]  = 0,
    [index(INPUT_KEY_PAUSE)]         = 0,
    [index(INPUT_KEY_0)]             = 0,
    [index(INPUT_KEY_1)]             = 0,
    [index(INPUT_KEY_2)]             = 0,
    [index(INPUT_KEY_3)]             = 0,
    [index(INPUT_KEY_4)]             = 0,
    [index(INPUT_KEY_5)]             = 0,
    [index(INPUT_KEY_6)]             = 0,
    [index(INPUT_KEY_7)]             = 0,
    [index(INPUT_KEY_8)]             = 0,
    [index(INPUT_KEY_9)]             = 0,
    [index(INPUT_KEY_MINUS)]         = 0,
    [index(INPUT_KEY_EQUALS)]        = 0,
    [index(INPUT_KEY_BACKSPACE)]     = 0,
    [index(INPUT_KEY_TAB)]           = 0,
    [index(INPUT_KEY_Q)]             = to_control('Q'),
    [index(INPUT_KEY_W)]             = to_control('W'),
    [index(INPUT_KEY_E)]             = to_control('E'),
    [index(INPUT_KEY_R)]             = to_control('R'),
    [index(INPUT_KEY_T)]             = to_control('T'),
    [index(INPUT_KEY_Y)]             = to_control('Y'),
    [index(INPUT_KEY_U)]             = to_control('U'),
    [index(INPUT_KEY_I)]             = to_control('I'),
    [index(INPUT_KEY_O)]             = to_control('O'),
    [index(INPUT_KEY_P)]             = to_control('P'),
    [index(INPUT_KEY_LEFT_BRACKET)]  = to_control('['),
    [index(INPUT_KEY_RIGHT_BRACKET)] = to_control(']'),
    [index(INPUT_KEY_RETURN)]        = 0,
    [index(INPUT_KEY_A)]             = to_control('A'),
    [index(INPUT_KEY_S)]             = to_control('S'),
    [index(INPUT_KEY_D)]             = to_control('D'),
    [index(INPUT_KEY_F)]             = to_control('F'),
    [index(INPUT_KEY_G)]             = to_control('G'),
    [index(INPUT_KEY_H)]             = to_control('H'),
    [index(INPUT_KEY_J)]             = to_control('J'),
    [index(INPUT_KEY_K)]             = to_control('K'),
    [index(INPUT_KEY_L)]             = to_control('L'),
    [index(INPUT_KEY_SEMICOLON)]     = 0,
    [index(INPUT_KEY_APOSTROPHE)]    = 0,
    [index(INPUT_KEY_BACKSLASH)]     = to_control('\\'),
    [index(INPUT_KEY_GRAVE)]         = 0,
    [index(INPUT_KEY_Z)]             = to_control('Z'),
    [index(INPUT_KEY_X)]             = to_control('X'),
    [index(INPUT_KEY_C)]             = to_control('C'),
    [index(INPUT_KEY_V)]             = to_control('V'),
    [index(INPUT_KEY_B)]             = to_control('B'),
    [index(INPUT_KEY_N)]             = to_control('N'),
    [index(INPUT_KEY_M)]             = to_control('M'),
    [index(INPUT_KEY_COMMA)]         = 0,
    [index(INPUT_KEY_PERIOD)]        = 0,
    [index(INPUT_KEY_SLASH)]         = 0,
    [index(INPUT_KEY_SPACE)]         = 0,
    [index(INPUT_KEY_LEFT)]          = 0,
    [index(INPUT_KEY_RIGHT)]         = 0,
    [index(INPUT_KEY_UP)]            = 0,
    [index(INPUT_KEY_DOWN)]          = 0,
    [index(INPUT_KEY_INSERT)]        = 0,
    [index(INPUT_KEY_DELETE)]        = 0,
    [index(INPUT_KEY_HOME)]          = 0,
    [index(INPUT_KEY_END)]           = 0,
    [index(INPUT_KEY_PAGE_UP)]       = 0,
    [index(INPUT_KEY_PAGE_DOWN)]     = 0,
    [index(INPUT_KEY_KP_DIVIDE)]     = 0,
    [index(INPUT_KEY_KP_ASTERISK)]   = 0,
    [index(INPUT_KEY_KP_MINUS)]      = 0,
    [index(INPUT_KEY_KP_PLUS)]       = 0,
    [index(INPUT_KEY_KP_ENTER)]      = 0,
    [index(INPUT_KEY_KP_7)]          = 0,
    [index(INPUT_KEY_KP_8)]          = 0,
    [index(INPUT_KEY_KP_9)]          = 0,
    [index(INPUT_KEY_KP_4)]          = 0,
    [index(INPUT_KEY_KP_5)]          = 0,
    [index(INPUT_KEY_KP_6)]          = 0,
    [index(INPUT_KEY_KP_1)]          = 0,
    [index(INPUT_KEY_KP_2)]          = 0,
    [index(INPUT_KEY_KP_3)]          = 0,
    [index(INPUT_KEY_KP_0)]          = 0,
    [index(INPUT_KEY_KP_PERIOD)]     = 0,
};
