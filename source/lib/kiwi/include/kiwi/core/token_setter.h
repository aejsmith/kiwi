/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Thread security token setter class.
 */

#pragma once

#include <kernel/thread.h>

namespace Kiwi {
    namespace Core {
        /**
         * RAII class for temporarily setting the calling thread's overridden
         * security token. The thread will be restored to the process-wide
         * security token when the object is destroyed.
         */
        class TokenSetter {
        public:
            TokenSetter();
            ~TokenSetter();

            TokenSetter(const TokenSetter &) = delete;
            TokenSetter& operator=(const TokenSetter &) = delete;

            TokenSetter(TokenSetter &&other) = delete;
            TokenSetter& operator=(TokenSetter &&other) = delete;

            /** @return             Whether the token is set. */
            bool isSet() const { return m_isSet; }

            status_t set(handle_t token);
            status_t set(const security_context_t *ctx);

            void unset();

        private:
            bool m_isSet;
        };

        inline TokenSetter::TokenSetter() :
            m_isSet (false)
        {}

        inline TokenSetter::~TokenSetter() {
            unset();
        }

        /**
         * Sets the calling thread's overridden security token to the given
         * token.
         *
         * @see                 kern_thread_set_token().
         */
        inline status_t TokenSetter::set(handle_t token) {
            unset();

            status_t ret = kern_thread_set_token(token);
            m_isSet = ret == STATUS_SUCCESS;
            return ret;
        }

        /**
         * Sets the calling thread's overridden security token to a new token
         * created from the given security context. The caller must have the
         * necessary privileges to create the token.
         *
         * @see                 kern_token_create().
         * @see                 kern_thread_set_token().
         */
        inline status_t TokenSetter::set(const security_context_t *ctx) {
            unset();

            handle_t token;
            status_t ret = kern_token_create(ctx, &token);
            if (ret == STATUS_SUCCESS) {
                ret = kern_thread_set_token(token);
                m_isSet = ret == STATUS_SUCCESS;
                kern_handle_close(token);
            }

            return ret;
        }

        /** Reset to the process-wide token if a token has been set. */
        inline void TokenSetter::unset() {
            if (m_isSet) {
                kern_thread_set_token(INVALID_HANDLE);
                m_isSet = false;
            }
        }
    }
}
