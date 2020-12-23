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
 * @brief               Terminal application.
 */

#include "terminal_app.h"
#include "terminal.h"

#include <core/log.h>

#include <kernel/object.h>
#include <kernel/status.h>

#include <stdlib.h>

TerminalApp g_terminalApp;

TerminalApp::TerminalApp() :
    m_activeTerminal (0)
{}

TerminalApp::~TerminalApp() {}

int TerminalApp::run() {
    auto terminal = new Terminal;
    if (!terminal->init()) {
        delete terminal;
        return EXIT_FAILURE;
    }

    m_terminals.emplace_back(terminal);

    // TODO: Input device enumeration.
    if (!m_keyboard.init("/class/input/0"))
        return EXIT_FAILURE;

    if (!m_framebuffer.init())
        return EXIT_FAILURE;

    while (!m_terminals.empty()) {
        /* Process any internally queued messages on the terminal connections
         * (if any messages were queued internally while waiting for a request
         * response, these won't be picked up by kern_object_wait()). TODO:
         * Better solution for this, e.g. core_connection provides an event
         * object to signal. */
        for (Terminal *terminal : m_terminals)
            terminal->handleMessages();

        status_t ret = kern_object_wait(m_events.data(), m_events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %d", ret);
            continue;
        }

        size_t numEvents = m_events.size();

        for (size_t i = 0; i < numEvents; ) {
            object_event_t &event = m_events[i];

            uint32_t flags = event.flags;
            event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);

            if (flags & OBJECT_EVENT_ERROR) {
                core_log(CORE_LOG_WARN, "error flagged on event %u for handle %u", event.event, event.handle);
            } else if (flags & OBJECT_EVENT_SIGNALLED) {
                auto handler = reinterpret_cast<EventHandler *>(event.udata);
                handler->handleEvent(event);
            }

            /* Calling the handler may change the event array, so we have to
             * handle this - start from the beginning. */
            if (numEvents != m_events.size()) {
                numEvents = m_events.size();
                i = 0;
            } else {
                i++;
            }
        }
    }

    return EXIT_SUCCESS;
}

void TerminalApp::addEvent(handle_t handle, unsigned id, EventHandler *handler) {
    object_event_t &event = m_events.emplace_back();

    event.handle = handle;
    event.event  = id;
    event.flags  = 0;
    event.data   = 0;
    event.udata  = handler;
}

void TerminalApp::removeEvents(EventHandler *handler) {
    for (auto it = m_events.begin(); it != m_events.end(); ) {
        if (reinterpret_cast<EventHandler *>(it->udata) == handler) {
            it = m_events.erase(it);
        } else {
            ++it;
        }
    }
}

void TerminalApp::removeTerminal(Terminal *terminal) {
    for (auto it = m_terminals.begin(); it != m_terminals.end(); ++it) {
        if (*it == terminal) {
            m_terminals.erase(it);
            break;
        }
    }
}

void TerminalApp::redraw() {
    
}

int main(int argc, char **argv) {
    return g_terminalApp.run();
}
