/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel object event loop class.
 */

#pragma once

#include <kernel/object.h>

#include <functional>
#include <vector>

namespace Kiwi {
    namespace Core {
        class EventRef;

        /**
         * Class implementing an event loop for waiting on and handling kernel
         * object events.
         *
         * This class is not thread-safe.
         */
        class EventLoop {
        public:
            /** Event handler function. */
            using EventHandler = std::function<void (const object_event_t &)>;

        public:
            EventLoop();
            ~EventLoop();

            EventRef addEvent(handle_t handle, unsigned id, uint32_t flags, EventHandler handler);

            status_t wait(uint32_t flags = 0, nstime_t timeout = -1);

        private:
            void removeEvent(EventHandler *handler);

        private:
            std::vector<object_event_t> m_events;

            /**
             * Version number used to detect handler changes while handling
             * events.
             */
            uint32_t m_version;

            friend class EventRef;
        };

        /**
         * Reference to an event in an EventLoop. Automatically removes the
         * event on destruction, and can also be removed explicitly.
         */
        class EventRef {
        public:
            EventRef();
            ~EventRef();

            EventRef(const EventRef &) = delete;
            EventRef& operator=(const EventRef &) = delete;

            EventRef(EventRef &&other);
            EventRef& operator=(EventRef &&other);

            void remove();

        private:
            EventLoop *m_loop;
            EventLoop::EventHandler *m_handler;

            friend class EventLoop;
        };
    }
}
