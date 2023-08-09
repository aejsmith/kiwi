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
 * @brief               RAII handle class.
 */

#pragma once

#include <kernel/object.h>

namespace Kiwi {
    namespace Core {
        /** RAII kernel handle wrapper. */
        class Handle {
        public:
            Handle();
            explicit Handle(handle_t handle);
            ~Handle();

            Handle(const Handle &) = delete;
            Handle& operator=(const Handle &) = delete;

            Handle(Handle &&other);
            Handle& operator=(Handle &&other);

            void close();

            handle_t *attach();
            void attach(handle_t handle);
            handle_t detach();

            operator handle_t() const   { return m_handle; }
            handle_t get() const        { return m_handle; }
            bool isValid() const        { return m_handle != INVALID_HANDLE; }

        private:
            handle_t m_handle;
        };

        /** Intialises as an invalid handle. */
        inline Handle::Handle() :
            m_handle (INVALID_HANDLE)
        {}

        /** Initialises from an existing handle. */
        inline Handle::Handle(handle_t handle) :
            m_handle (handle)
        {}

        /** Takes ownership of another handle (other will be made invalid). */
        inline Handle::Handle(Handle &&other) :
            m_handle (other.m_handle)
        {
            other.m_handle = INVALID_HANDLE;
        }

        /** Closes the current handle (if any). */
        inline Handle::~Handle() {
            close();
        }

        /** Take ownership of another handle (other will be made invalid). */
        inline Handle& Handle::operator=(Handle &&other) {
            close();

            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE;
            return *this;
        }

        /**
         * Attaches to a new handle. If an existing handle is open then it will
         * be closed. This returns a pointer to the internal handle_t which
         * should be written into - this is for use with kernel functions that
         * return handles via pointer.
         *
         * Note that all kernel APIs are guaranteed to either not write to the
         * given handle pointer or write INVALID_HANDLE to it upon failure.
         * This means that when a kernel call using this function for the handle
         * return fails, the Handle is guaranteed to be left in an invalid
         * state (and any previous handle it held will be closed).
         */
        inline handle_t *Handle::attach() {
            close();
            return &m_handle;
        }

        /**
         * Attaches to a new handle. If an existing handle is open then it will
         * be closed.
         */
        inline void Handle::attach(handle_t handle) {
            close();
            m_handle = handle;
        }

        /** Closes the current handle (if any), and sets this handle as invalid. */
        inline void Handle::close() {
            if (m_handle != INVALID_HANDLE) {
                kern_handle_close(m_handle);
                m_handle = INVALID_HANDLE;
            }
        }

        /** Releases ownership of the handle without closing it. */
        inline handle_t Handle::detach() {
            handle_t ret = m_handle;
            m_handle = INVALID_HANDLE;
            return ret;
        }
    }
}
