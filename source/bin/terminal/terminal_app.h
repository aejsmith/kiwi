/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal application.
 */

#pragma once

#include <device/input.h>

#include <kiwi/core/event_loop.h>

#include <vector>

#include "font.h"
#include "framebuffer.h"
#include "keyboard.h"

class TerminalWindow;

class TerminalApp {
public:
    TerminalApp();
    ~TerminalApp();

    Kiwi::Core::EventLoop &eventLoop()      { return m_eventLoop; }
    TerminalWindow &activeWindow() const    { return *m_windows[m_activeWindow]; }
    Framebuffer &framebuffer()              { return m_framebuffer; }
    Keyboard &keyboard()                    { return m_keyboard; }
    Font &font()                            { return m_font; }

    int run();

    void removeWindow(TerminalWindow *window);

    void handleInput(const input_event_t &event);
    void redraw();

private:
    using WindowArray = std::vector<TerminalWindow *>;

private:
    Kiwi::Core::EventLoop m_eventLoop;

    WindowArray m_windows;
    size_t m_activeWindow;

    Framebuffer m_framebuffer;
    Keyboard m_keyboard;
    Font m_font;
};

extern TerminalApp g_terminalApp;
