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

#pragma once

#include <device/input.h>

/**
 * Class to map input device key events to characters.
 *
 * TODO: This functionality will eventually be handled by the window service,
 * when we have a proper GUI.
 * TODO: Keyboard layout support.
 */
class Keymap {
public:
    Keymap();

    size_t map(input_event_t &event, uint8_t buf[4]);

private:
    enum Modifiers : uint32_t {
        kLeftCtrlModifier   = (1<<0),
        kRightCtrlModifier  = (1<<1),

        kCtrlModifiers      = kLeftCtrlModifier | kRightCtrlModifier,

        kLeftAltModifier    = (1<<2),
        kRightAltModifier   = (1<<3),

        kAltModifiers       = kLeftAltModifier | kRightAltModifier,

        kLeftShiftModifier  = (1<<4),
        kRightShiftModifier = (1<<5),

        kShiftModifiers     = kLeftShiftModifier | kRightShiftModifier,

        kCapsLockModifier   = (1<<6),
    };

private:
    uint32_t m_modifiers;
};
