/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Terminal application.
 */

#pragma once

#include "event_handler.h"
#include "font.h"
#include "framebuffer.h"
#include "keyboard.h"

#include <device/input.h>

#include <vector>

class TerminalWindow;

class TerminalApp {
public:
    TerminalApp();
    ~TerminalApp();

    TerminalWindow &activeWindow() const    { return *m_windows[m_activeWindow]; }
    Framebuffer &framebuffer()              { return m_framebuffer; }
    Keyboard &keyboard()                    { return m_keyboard; }
    Font &font()                            { return m_font; }

    int run();

    void addEvent(handle_t handle, unsigned id, EventHandler *handler);
    void removeEvents(EventHandler *handler);

    void removeWindow(TerminalWindow *window);

    void handleInput(const input_event_t &event);
    void redraw();

private:
    using WindowArray = std::vector<TerminalWindow *>;
    using EventArray  = std::vector<object_event_t>;

private:
    WindowArray m_windows;
    size_t m_activeWindow;

    Framebuffer m_framebuffer;
    Keyboard m_keyboard;
    Font m_font;

    EventArray m_events;
};

extern TerminalApp g_terminalApp;
