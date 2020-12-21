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
 * @brief               Keyboard map.
 */

#include "keymap.h"

#include <core/utility.h>

#include <ctype.h>

extern uint8_t g_keyTable[256];
extern uint8_t g_keyTableShift[256];
extern uint8_t g_keyTableCtrl[256];

Keymap::Keymap() :
    m_modifiers (0)
{}

/** Map an input event to a UTF-8 character.
 * @param event         Event to handle.
 * @param buf           Output buffer.
 * @return              Character length (0 if no corresponding character). */
size_t Keymap::map(input_event_t &event, uint8_t buf[4]) {
    // TODO: Actually support UTF multibyte characters when we have a need for
    // it.

    auto handleModifier = [&] (int32_t key, uint32_t modifier) {
        if (event.value == key) {
            if (event.type == INPUT_EVENT_KEY_DOWN) {
                m_modifiers |= modifier;
            } else {
                m_modifiers &= ~modifier;
            }
        }
    };

    handleModifier(INPUT_KEY_LEFT_CTRL,   kLeftCtrlModifier);
    handleModifier(INPUT_KEY_RIGHT_CTRL,  kRightCtrlModifier);
    handleModifier(INPUT_KEY_LEFT_ALT,    kLeftAltModifier);
    handleModifier(INPUT_KEY_RIGHT_ALT,   kRightAltModifier);
    handleModifier(INPUT_KEY_LEFT_SHIFT,  kLeftShiftModifier);
    handleModifier(INPUT_KEY_RIGHT_SHIFT, kRightShiftModifier);

    size_t len = 0;

    if (event.type == INPUT_EVENT_KEY_DOWN) {
        if (event.value == INPUT_KEY_CAPS_LOCK)
            m_modifiers ^= kCapsLockModifier;

        uint8_t page   = event.value >> 16;
        uint16_t usage = event.value & 0xffff;

        /* Ignore other pages for now. */
        if (page == 0x07 && usage <= core_array_size(g_keyTable)) {
            uint8_t *table = (m_modifiers & kShiftModifiers) ? g_keyTableShift : g_keyTable;

            if (m_modifiers & kCtrlModifiers && g_keyTableCtrl[usage]) {
            buf[len++] = g_keyTableCtrl[usage];
            } else if (table[usage]) {
                if (m_modifiers & kCapsLockModifier && isalpha(table[usage])) {
                    buf[len++] = toupper(table[usage]);
                } else {
                    buf[len++] = table[usage];
                }
            }
        }
    }

    return len;
}
