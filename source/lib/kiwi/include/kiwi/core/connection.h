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
 * @brief               IPC connection class.
 */

#pragma once

#include <core/service.h>

#include <kernel/status.h>

#include <kiwi/core/handle.h>
#include <kiwi/core/message.h>

namespace Kiwi {
    namespace Core {
        /**
         * IPC connection class. This is a C++ wrapper of the C core_connection
         * API.
         *
         * @see                 core_connection_t.
         */
        class Connection {
        public:
            /** Connection flags. */
            enum : uint32_t {
                kReceiveRequests        = CORE_CONNECTION_RECEIVE_REQUESTS,
                kReceiveSignals         = CORE_CONNECTION_RECEIVE_SIGNALS,
                kReceiveSecurity        = CORE_CONNECTION_RECEIVE_SECURITY,
            };

        public:
            Connection();
            explicit Connection(core_connection_t *conn);
            ~Connection();

            Connection(const Connection &) = delete;
            Connection& operator=(const Connection &) = delete;

            Connection(Connection &&other);
            Connection& operator=(Connection &&other);

            void attach(core_connection_t *conn);
            core_connection_t *detach();

            core_connection_t *get() const  { return m_conn; }
            bool isValid() const            { return m_conn != nullptr; }

            /**
             * core_connection API wrappers.
             */

            bool create(Kiwi::Core::Handle handle, uint32_t flags);
            bool create(handle_t handle, uint32_t flags);
            status_t open(handle_t port, nstime_t timeout, uint32_t flags);
            status_t openService(const char *name, uint32_t service_flags, uint32_t conn_flags);
            void close();
            void destroy();

            handle_t handle() const;
            bool isActive() const;

            status_t signal(Message &signal);
            status_t request(Message &request, Message &_reply);
            status_t reply(Message &reply);
            status_t receive(nstime_t timeout, Message &_message);

        private:
            core_connection_t *m_conn;
        };

        /** Intialises as an invalid connection. */
        inline Connection::Connection() :
            m_conn (nullptr)
        {}

        /** Initialises from an existing connection. */
        inline Connection::Connection(core_connection_t *conn) :
            m_conn (conn)
        {}

        /** Takes ownership of another connection (other will be made invalid). */
        inline Connection::Connection(Connection &&other) :
            m_conn (other.m_conn)
        {
            other.m_conn = nullptr;
        }

        /** Closes the current connection (if any). */
        inline Connection::~Connection() {
            close();
        }

        /** Take ownership of another connection (other will be made invalid). */
        inline Connection& Connection::operator=(Connection &&other) {
            close();

            m_conn = other.m_conn;
            other.m_conn = nullptr;
            return *this;
        }

        /**
         * Attaches to a new connection. If an existing connection is open then
         * it will be closed.
         */
        inline void Connection::attach(core_connection_t *conn) {
            close();
            m_conn = conn;
        }

        /** Releases ownership of the connection without closing it. */
        inline core_connection_t *Connection::detach() {
            core_connection_t *ret = m_conn;
            m_conn = nullptr;
            return ret;
        }

        /**
         * Creates a new connection from an existing connection handle. If an
         * existing connection is open then it will be closed.
         *
         * @see                 core_connection_create().
         *
         * @return              Whether the connection was successfully
         *                      created.
         */
        inline bool Connection::create(Kiwi::Core::Handle handle, uint32_t flags) {
            close();

            m_conn = core_connection_create(handle, flags);
            if (isValid()) {
                handle.detach();
                return true;
            } else {
                return false;
            }
        }

        /**
         * Creates a new connection from an existing connection handle. If an
         * existing connection is open then it will be closed.
         *
         * @see                 core_connection_create().
         *
         * @return              Whether the connection was successfully
         *                      created.
         */
        inline bool Connection::create(handle_t handle, uint32_t flags) {
            close();

            m_conn = core_connection_create(handle, flags);
            return isValid();
        }

        /**
         * Creates a new connection by connecting to a port. If an existing
         * connection is open then it will be closed.
         *
         * @see                core_connection_open().
         */
        inline status_t Connection::open(handle_t port, nstime_t timeout, uint32_t flags) {
            close();

            return core_connection_open(port, timeout, flags, &m_conn);
        }

        /**
         * Creates a new connection by connecting to a service. If an existing
         * connection is open then it will be closed.
         *
         * @see                 core_service_open().
         */
        inline status_t Connection::openService(const char *name, uint32_t service_flags, uint32_t conn_flags) {
            close();

            return core_service_open(name, service_flags, conn_flags, &m_conn);
        }

        /**
         * Closes the current connection (if any) and sets this connection as
         * invalid.
         *
         * @see                 core_connection_close().
         */
        inline void Connection::close() {
            if (m_conn) {
                core_connection_close(m_conn);
                m_conn = nullptr;
            }
        }

        /**
         * Destroys the current connection (if any), assuming it's underlying
         * handle has already been closed, and sets this connection as invalid.
         *
         * @see                 core_connection_destroy().
         */
        inline void Connection::destroy() {
            if (m_conn) {
                core_connection_destroy(m_conn);
                m_conn = nullptr;
            }
        }

        /** @see                core_connection_handle(). */
        inline handle_t Connection::handle() const {
            return core_connection_handle(m_conn);
        }

        /** @see                core_connection_is_active(). */
        inline bool Connection::isActive() const {
            return core_connection_is_active(m_conn);
        }

        /** @see                core_connection_signal(). */
        inline status_t Connection::signal(Message &signal) {
            return core_connection_signal(m_conn, signal.get());
        }

        /** @see                core_connection_request(). */
        inline status_t Connection::request(Message &request, Message &_reply) {
            core_message_t *reply;
            status_t ret = core_connection_request(m_conn, request.get(), &reply);
            if (ret == STATUS_SUCCESS)
                _reply.attach(reply);

            return ret;
        }

        /** @see                core_connection_reply(). */
        inline status_t Connection::reply(Message &reply) {
            return core_connection_reply(m_conn, reply.get());
        }

        /** @see                core_connection_receive(). */
        inline status_t Connection::receive(nstime_t timeout, Message &_message) {
            core_message_t *message;
            status_t ret = core_connection_receive(m_conn, timeout, &message);
            if (ret == STATUS_SUCCESS)
                _message.attach(message);

            return ret;
        }
    }
}
