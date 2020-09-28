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
 * @brief               Terminal service IPC protocol.
 */

#pragma once

#define TERMINAL_SERVICE_NAME "org.kiwi.terminal"

/** Terminal service message IDs. */
enum {
    /**
     * Retrieve a file handle for the terminal.
     *
     * Request:
     *  - Data = terminal_request_open_handle_t
     *
     * Reply:
     *  - Data = terminal_reply_open_handle_t
     *  - Handle = File handle (if successful)
     */
    TERMINAL_REQUEST_OPEN_HANDLE = 0,

    /**
     * Supply input to the terminal.
     *
     * Request:
     *  - Data = Input data
     *
     * Reply:
     *  - Data = terminal_reply_input_t
     */
    TERMINAL_REQUEST_INPUT = 1,

    /**
     * Receive output from the terminal.
     *
     * Signal:
     *  - Data = Output data.
     */
    TERMINAL_SIGNAL_OUTPUT = 2
};

typedef struct terminal_request_open_handle {
    uint32_t access;
} terminal_request_open_handle_t;

typedef struct terminal_reply_open_handle {
    status_t result;
} terminal_reply_open_handle_t;

typedef struct terminal_reply_input {
    status_t result;
} terminal_reply_input_t;
