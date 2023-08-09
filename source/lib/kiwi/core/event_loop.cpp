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
 * @brief               Kernel object event loop class.
 *
 * TODO:
 *  - Use a watcher object once that is implemented.
 */

#include <core/log.h>

#include <kernel/status.h>

#include <kiwi/core/event_loop.h>

using namespace Kiwi::Core;

EventLoop::EventLoop() :
    m_version (0)
{}

EventLoop::~EventLoop() {}

/**
 * Adds a new event to the event loop to be waited on the next time that wait()
 * is called.
 *
 * @param handle        Handle to wait on.
 * @param id            ID of the event to wait for.
 * @param flags         Object event flags (OBJECT_EVENT_*).
 * @param handler       Handler function to be called when the event is
 *                      signalled.
 *
 * @return              Event reference that can be used to remove the event
 *                      later on.
 */
EventRef EventLoop::addEvent(handle_t handle, unsigned id, uint32_t flags, EventHandler handler) {
    EventHandler *copy = new EventHandler(std::move(handler));

    object_event_t &event = m_events.emplace_back();

    event.handle = handle;
    event.event  = id;
    event.flags  = flags & ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);
    event.data   = 0;
    event.udata  = copy;

    EventRef ref;
    ref.m_loop    = this;
    ref.m_handler = copy;

    m_version++;

    return ref;
}

/** Removes an event from the loop. */
void EventLoop::removeEvent(EventHandler *handler) {
    for (auto it = m_events.begin(); it != m_events.end(); ++it) {
        if (reinterpret_cast<EventHandler *>(it->udata) == handler) {
            m_events.erase(it);
            m_version++;
            return;
        }
    }

    core_log(CORE_LOG_WARN, "attempting to remove unknown handler from EventLoop");
}

/**
 * Waits for any of the registered events to occur and calls their handlers.
 * This function only performs one iteration and handles any events that did
 * occur. It should be called in a loop to repeatedly wait for and handle
 * events.
 *
 * @param flags         Flags to pass to kern_object_wait() (OBJECT_WAIT_*).
 * @param timeout       Timeout to pass to kern_object_wait().
 *
 * @return              Status returned from kern_object_wait().
 */
status_t EventLoop::wait(uint32_t flags, nstime_t timeout) {
    status_t ret = kern_object_wait(m_events.data(), m_events.size(), flags, timeout);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_WARN, "failed to wait for events: %d", ret);
        return ret;
    }

    uint32_t version = m_version;
    for (size_t i = 0; i < m_events.size(); ) {
        object_event_t &event = m_events[i];

        uint32_t flags = event.flags;
        event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);

        if (flags & OBJECT_EVENT_ERROR) {
            core_log(CORE_LOG_WARN, "error flagged on event %u for handle %u", event.event, event.handle);
        } else if (flags & OBJECT_EVENT_SIGNALLED) {
            auto handler = reinterpret_cast<EventHandler *>(event.udata);
            (*handler)(event);
        }

        /* Calling the handler may change the event array. This is indicated by
         * the version changing. In this case, restart from the beginning of
         * the array to make sure we don't miss anything. */
        if (version != m_version) {
            version = m_version;
            i = 0;
        } else {
            i++;
        }
    }

    return STATUS_SUCCESS;
}

/** Initialises as an empty reference. */
EventRef::EventRef() :
    m_loop      (nullptr),
    m_handler   (nullptr)
{}

/** Removes the event that this reference is for. */
EventRef::~EventRef() {
    remove();
}

/** Takes ownership of another event. */
EventRef::EventRef(EventRef &&other) :
    m_loop      (other.m_loop),
    m_handler   (other.m_handler)
{
    other.m_loop    = nullptr;
    other.m_handler = nullptr;
}

/** Takes ownership of another event (any existing reference will be removed). */
EventRef& EventRef::operator=(EventRef &&other) {
    remove();

    std::swap(m_loop, other.m_loop);
    std::swap(m_handler, other.m_handler);
    return *this;
}

/** Removes the event that this reference is for. */
void EventRef::remove() {
    if (m_loop) {
        m_loop->removeEvent(m_handler);

        delete m_handler;

        m_loop    = nullptr;
        m_handler = nullptr;
    }
}
