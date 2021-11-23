/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               UDP protocol implementation.
 */

#include <mm/malloc.h>

#include <net/udp.h>

#include <status.h>

static void udp_socket_close(socket_t *_socket) {
    udp_socket_t *socket = cast_udp_socket(cast_net_socket(_socket));

    kfree(socket);
}

static const socket_ops_t udp_socket_ops = {
    .close = udp_socket_close,
};

/** Creates a UDP socket. */
status_t udp_socket_create(sa_family_t family, socket_t **_socket) {
    udp_socket_t *socket = kmalloc(sizeof(udp_socket_t), MM_KERNEL);

    socket->net.socket.ops = &udp_socket_ops;

    *_socket = &socket->net.socket;
    return STATUS_SUCCESS;
}
