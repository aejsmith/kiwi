/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Service manager IPC protocol.
 */

#pragma once

/** Service manager message IDs. */
enum {
    TEST_SIGNAL_START = 0,
    TEST_REQUEST_PING = 1,
};

#define TEST_STRING_LEN         16

typedef struct test_request_ping {
    uint32_t index;
    char string[TEST_STRING_LEN];
} test_request_ping_t;
