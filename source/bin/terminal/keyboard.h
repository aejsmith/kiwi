/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
