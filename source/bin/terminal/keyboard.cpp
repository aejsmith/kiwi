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
 * @brief               Keyboard input class.
 */

#include "keyboard.h"
#include "terminal_app.h"
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
    g_terminalApp.removeEvents(this);

    if (m_device)
        device_close(m_device);
}

bool Keyboard::init(const char *path) {
    status_t ret = input_device_open(path, FILE_ACCESS_READ, FILE_NONBLOCK, &m_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open input device: %" PRId32, ret);
        return false;
    } else if (input_device_type(m_device) != INPUT_DEVICE_KEYBOARD) {
        core_log(CORE_LOG_ERROR, "input device is not a keyboard", ret);
        return false;
    }

    g_terminalApp.addEvent(device_handle(m_device), FILE_EVENT_READABLE, this);

    return true;
}

void Keyboard::handleEvent(const object_event_t &event) {
    status_t ret;

    assert(event.handle == device_handle(m_device));
    assert(event.event == FILE_EVENT_READABLE);

    /* Batch up events if we can to reduce number of messages. */
    static constexpr size_t kBatchSize = 128;
    uint8_t buf[kBatchSize];

    size_t len = 0;
    bool done  = false;
    while (!done) {
        input_event_t event;
        ret = input_device_read_event(m_device, &event);
        if (ret == STATUS_SUCCESS) {
            len += map(event, &buf[len]);
        } else {
            if (ret != STATUS_WOULD_BLOCK)
                core_log(CORE_LOG_ERROR, "failed to read input device: %" PRId32, ret);

            done = true;
        }

        if (len > 0 && (done || kBatchSize - len < 4)) {
            g_terminalApp.activeTerminal()->sendInput(buf, len);
            len = 0;
        }
    }
}

/** Map an input event to a UTF-8 character.
 * @return              Character length (0 if no corresponding character). */
size_t Keyboard::map(const input_event_t &event, uint8_t buf[4]) {
    // TODO: This will eventually be handled by the window server.

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
