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
 * @brief               Service manager IPC protocol.
 */

#pragma once

/** Service manager message IDs. */
enum {
    /**
     * Connect to a service.
     *
     * Request:
     *  - Data = service_manager_request_connect_t
     *
     * Reply:
     *  - Data = service_manager_reply_connect_t
     *  - Handle = Service port (if successful)
     */
    SERVICE_MANAGER_REQUEST_CONNECT = 0,

    /**
     * Register a service port.
     *
     * Request:
     *  - Handle = Service port
     *
     * Reply:
     *  - Data = service_manager_reply_register_port_t
     */
    SERVICE_MANAGER_REQUEST_REGISTER_PORT = 1,
};

typedef struct service_manager_request_connect {
    uint32_t flags;
    char name[];
} service_manager_request_connect_t;

typedef struct service_manager_reply_connect {
    status_t result;
} service_manager_reply_connect_t;

typedef struct service_manager_reply_register_port {
    status_t result;
} service_manager_reply_register_port_t;
