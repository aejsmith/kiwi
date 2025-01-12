/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
