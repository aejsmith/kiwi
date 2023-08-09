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
 * @brief               Keyboard input class.
 */

#pragma once

#include <device/input.h>

#include <kiwi/core/event_loop.h>

/** Class handling keyboard input. */
class Keyboard {
public:
    enum Modifiers : uint32_t {
        kModifier_LeftCtrl      = (1<<0),
        kModifier_RightCtrl     = (1<<1),

        kModifiers_Ctrl         = kModifier_LeftCtrl | kModifier_RightCtrl,

        kModifier_LeftAlt       = (1<<2),
        kModifier_RightAlt      = (1<<3),

        kModifiers_Alt          = kModifier_LeftAlt | kModifier_RightAlt,

        kModifier_LeftShift     = (1<<4),
        kModifier_RightShift    = (1<<5),

        kModifiers_Shift        = kModifier_LeftShift | kModifier_RightShift,

        kModifier_CapsLock      = (1<<6),
    };

public:
    Keyboard();
    ~Keyboard();

    uint32_t modifiers() const { return m_modifiers; }

    bool init(const char *path);

    size_t map(const input_event_t &event, uint8_t buf[4]);

private:
    void handleReadableEvent();

private:
    input_device_t *m_device;
    uint32_t m_modifiers;

    Kiwi::Core::EventRef m_readableEvent;
};
