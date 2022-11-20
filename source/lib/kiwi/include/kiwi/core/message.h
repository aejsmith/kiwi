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
 * @brief               IPC message class.
 */

#pragma once

#include <core/ipc.h>

#include <kiwi/core/handle.h>

namespace Kiwi {
    namespace Core {
        /**
         * IPC message class. This is a C++ wrapper of the C core_message API.
         *
         * @see                 core_message_t.
         */
        class Message {
        public:
            /** Message types. */
            enum Type {
                kSignal         = CORE_MESSAGE_SIGNAL,
                kRequest        = CORE_MESSAGE_REQUEST,
                kReply          = CORE_MESSAGE_REPLY,
            };

            /** Message flags. */
            enum : uint32_t {
                kSendSecurity   = CORE_MESSAGE_SEND_SECURITY,
            };

        public:
            Message();
            explicit Message(core_message_t *message);
            ~Message();

            Message(const Message &) = delete;
            Message& operator=(const Message &) = delete;

            Message(Message &&other);
            Message& operator=(Message &&other);

            void attach(core_message_t *message);
            core_message_t *detach();

            core_message_t *get() const { return m_message; }
            bool isValid() const        { return m_message != nullptr; }

            /**
             * core_message API wrappers.
             */

            bool createSignal(uint32_t id, size_t size, uint32_t flags = 0);
            bool createRequest(uint32_t id, size_t size, uint32_t flags = 0);
            bool createReply(const Message &request, size_t size, uint32_t flags = 0);
            void destroy();

            Type type() const;
            uint32_t id() const;
            size_t size() const;
            nstime_t timestamp() const;
            const security_context_t *security() const;
            template <typename T = void> T *data();
            template <typename T = void> const T *data() const;

            void attachHandle(handle_t handle, bool own = false);
            void attachHandle(Handle &&handle);
            Handle detachHandle();

        private:
            core_message_t *m_message;
        };

        /** Intialises as an invalid message. */
        inline Message::Message() :
            m_message (nullptr)
        {}

        /** Initialises from an existing message. */
        inline Message::Message(core_message_t *message) :
            m_message (message)
        {}

        /** Takes ownership of another message (other will be made invalid). */
        inline Message::Message(Message &&other) :
            m_message (other.m_message)
        {
            other.m_message = nullptr;
        }

        /** Destroys the current message (if any). */
        inline Message::~Message() {
            destroy();
        }

        /** Take ownership of another message (other will be made invalid). */
        inline Message& Message::operator=(Message &&other) {
            destroy();

            m_message = other.m_message;
            other.m_message = nullptr;
            return *this;
        }

        /**
         * Attaches to a new message. If there is an existing message then it
         * will be destroyed.
         */
        inline void Message::attach(core_message_t *message) {
            destroy();
            m_message = message;
        }

        /** Releases ownership of the message without destroying it. */
        inline core_message_t *Message::detach() {
            core_message_t *ret = m_message;
            m_message = nullptr;
            return ret;
        }

        /**
         * Creates a new signal message. If there is an existing message then
         * it will be destroyed.
         *
         * @see                 core_message_create_signal().
         *
         * @return              Whether the message was successfully created.
         */
        inline bool Message::createSignal(uint32_t id, size_t size, uint32_t flags) {
            destroy();

            m_message = core_message_create_signal(id, size, flags);
            return isValid();
        }

        /**
         * Creates a new request message. If there is an existing message then
         * it will be destroyed.
         *
         * @see                 core_message_create_request().
         *
         * @return              Whether the message was successfully created.
         */
        inline bool Message::createRequest(uint32_t id, size_t size, uint32_t flags) {
            destroy();

            m_message = core_message_create_request(id, size, flags);
            return isValid();
        }

        /**
         * Creates a new reply message. If there is an existing message then
         * it will be destroyed.
         *
         * @see                 core_message_create_reply().
         *
         * @return              Whether the message was successfully created.
         */
        inline bool Message::createReply(const Message &request, size_t size, uint32_t flags) {
            destroy();

            m_message = core_message_create_reply(request.get(), size, flags);
            return isValid();
        }

        /**
         * Destroys the existing message (if any) and sets this message as
         * invalid.
         *
         * @see                 core_message_destroy().
         */
        inline void Message::destroy() {
            if (m_message) {
                core_message_destroy(m_message);
                m_message = nullptr;
            }
        }

        /** @see                core_message_type(). */
        inline Message::Type Message::type() const {
            return static_cast<Type>(core_message_type(m_message));
        }

        /** @see                core_message_id(). */
        inline uint32_t Message::id() const {
            return core_message_id(m_message);
        }

        /** @see                core_message_size(). */
        inline size_t Message::size() const {
            return core_message_size(m_message);
        }

        /** @see                core_message_timestamp(). */
        inline nstime_t Message::timestamp() const {
            return core_message_timestamp(m_message);
        }

        /** @see                core_message_security(). */
        inline const security_context_t *Message::security() const {
            return core_message_security(m_message);
        }

        /** @see                core_message_data(). */
        template <typename T>
        inline T *Message::data() {
            return reinterpret_cast<T *>(core_message_data(m_message));
        }

        /** @see                core_message_data(). */
        template <typename T>
        inline const T *Message::data() const {
            return reinterpret_cast<T *>(core_message_data(m_message));
        }

        /** @see                core_message_attach_handle(). */
        inline void Message::attachHandle(handle_t handle, bool own) {
            core_message_attach_handle(m_message, handle, own);
        }

        /** @see                core_message_attach_handle(). */
        inline void Message::attachHandle(Handle &&handle) {
            core_message_attach_handle(m_message, handle.detach(), true);
        }

        /** @see                core_message_detach_handle(). */
        inline Handle Message::detachHandle() {
            return Handle(core_message_detach_handle(m_message));
        }
    }
}
