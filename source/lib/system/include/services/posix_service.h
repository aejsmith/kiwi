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
 * @brief               POSIX service IPC protocol.
 */

#pragma once

#include <stdint.h>

#define POSIX_SERVICE_NAME "org.kiwi.posix"

/** POSIX service message IDs. */
enum {
    /**
     * Sends a signal to a process.
     *
     * Request:
     *  - Data  = posix_request_kill_t
     *  - Flags = CORE_MESSAGE_SEND_SECURITY
     *
     * Reply:
     *  - Data = posix_reply_kill_t
     */
    POSIX_REQUEST_KILL = 0,
};

typedef struct posix_request_kill {
    int32_t pid;
    int32_t num;
} posix_request_kill_t;

typedef struct posix_reply_kill {
    int32_t err;
} posix_reply_kill_t;
