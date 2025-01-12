/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Keyboard input class.
 */

#include "keyboard.h"
#include "terminal_app.h"
#include "terminal_window.h"
#include "terminal.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/status.h>

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>

extern uint8_t g_keyTable[256];
extern uint8_t g_keyTableShift[256];
extern uint8_t g_keyTableCtrl[256];

Keyboard::Keyboard() :
    m_device    (nullptr),
    m_modifiers (0)
{}

Keyboard::~Keyboard() {
    if (m_device)
        device_close(m_device);
}

bool Keyboard::init(const char *path) {
    status_t ret = input_device_open(path, FILE_ACCESS_READ, FILE_NONBLOCK, &m_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open input device: %" PRId32, ret);
        return false;
    } else if (input_device_type(m_device) != INPUT_DEVICE_KEYBOARD) {
        core_log(CORE_LOG_ERROR, "input device is not a keyboard");
        return false;
    }

    m_readableEvent = g_terminalApp.eventLoop().addEvent(
        device_handle(m_device), FILE_EVENT_READABLE, 0,
        [this] (const object_event_t &event) { handleReadableEvent(); });

    return true;
}

void Keyboard::handleReadableEvent() {
    bool done = false;
    while (!done) {
        input_event_t event;
        status_t ret = input_device_read_event(m_device, &event);
        if (ret == STATUS_SUCCESS) {
            auto handleModifier = [&] (int32_t key, uint32_t modifier) {
                if (event.value == key) {
                    if (event.type == INPUT_EVENT_KEY_DOWN) {
                        m_modifiers |= modifier;
                    } else {
                        m_modifiers &= ~modifier;
                    }
                }
            };

            handleModifier(INPUT_KEY_LEFT_CTRL,   kModifier_LeftCtrl);
            handleModifier(INPUT_KEY_RIGHT_CTRL,  kModifier_RightCtrl);
            handleModifier(INPUT_KEY_LEFT_ALT,    kModifier_LeftAlt);
            handleModifier(INPUT_KEY_RIGHT_ALT,   kModifier_RightAlt);
            handleModifier(INPUT_KEY_LEFT_SHIFT,  kModifier_LeftShift);
            handleModifier(INPUT_KEY_RIGHT_SHIFT, kModifier_RightShift);

            g_terminalApp.handleInput(event);
        } else {
            if (ret != STATUS_WOULD_BLOCK)
                core_log(CORE_LOG_ERROR, "failed to read input device: %" PRId32, ret);

            done = true;
        }
    }
}

/** Map an input event to a UTF-8 character.
 * @return              Character length (0 if no corresponding character). */
size_t Keyboard::map(const input_event_t &event, uint8_t buf[4]) {
    // TODO: This will eventually be handled by the window server.

    // TODO: Actually support UTF multibyte characters when we have a need for
    // it.

    size_t len = 0;

    if (event.type == INPUT_EVENT_KEY_DOWN) {
        if (event.value == INPUT_KEY_CAPS_LOCK)
            m_modifiers ^= kModifier_CapsLock;

        uint8_t page   = event.value >> 16;
        uint16_t usage = event.value & 0xffff;

        /* Ignore other pages for now. */
        if (page == 0x07 && usage <= core_array_size(g_keyTable)) {
            uint8_t *table = (m_modifiers & kModifiers_Shift) ? g_keyTableShift : g_keyTable;

            if (m_modifiers & kModifiers_Ctrl && g_keyTableCtrl[usage]) {
                buf[len++] = g_keyTableCtrl[usage];
            } else if (table[usage]) {
                if (m_modifiers & kModifier_CapsLock && isalpha(table[usage])) {
                    buf[len++] = toupper(table[usage]);
                } else {
                    buf[len++] = table[usage];
                }
            }
        }
    }

    return len;
}
