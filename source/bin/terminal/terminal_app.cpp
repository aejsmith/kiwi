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
 * @brief               Terminal application.
 */
#include "font.h"
#include "terminal_app.h"
#include "terminal_window.h"
#include "terminal.h"

#include <core/log.h>

#include <kernel/object.h>
#include <kernel/status.h>

#include <stdlib.h>

TerminalApp g_terminalApp;

TerminalApp::TerminalApp() :
    m_activeWindow (0)
{}

TerminalApp::~TerminalApp() {}

int TerminalApp::run() {
    /* Set the TERM value for clients to inherit. */
    setenv("TERM", "xterm-color", 1);

    // TODO: Input device enumeration.
    if (!m_keyboard.init("/class/input/0"))
        return EXIT_FAILURE;

    if (!m_framebuffer.init())
        return EXIT_FAILURE;

    if (!m_font.init("/system/fonts/source-code-pro/SourceCodePro-Medium.ttf", 9))
        return EXIT_FAILURE;

    auto window = new TerminalWindow;
    if (!window->init()) {
        delete window;
        return EXIT_FAILURE;
    }

    m_windows.emplace_back(window);

    while (!m_windows.empty()) {
        for (TerminalWindow *window : m_windows) {
            /* Flush any buffered input. */
            window->terminal().flushInput();

            /* Process any internally queued messages on the terminal
             * connections (if any messages were queued internally while waiting
             * for a request response, these won't be picked up by
             * kern_object_wait()). TODO: Better solution for this, e.g.
             * core_connection provides a condition object to wait on. */
            window->terminal().handleMessages();
        }

        m_eventLoop.wait();
    }

    return EXIT_SUCCESS;
}

void TerminalApp::removeWindow(TerminalWindow *window) {
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (*it == window) {
            m_windows.erase(it);
            break;
        }
    }
}

void TerminalApp::redraw() {
    activeWindow().redraw();
}

void TerminalApp::handleInput(const input_event_t &event) {
    activeWindow().handleInput(event);
}

int main(int argc, char **argv) {
    return g_terminalApp.run();
}
