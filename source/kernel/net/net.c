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
 * @brief               Network stack module main functions.
 */

#include <device/net.h>

#include <net/net.h>
#include <net/packet.h>

#include <module.h>
#include <status.h>

static status_t net_init(void) {
    net_packet_cache_init();
    net_device_class_init();

    return STATUS_SUCCESS;
}

static status_t net_unload(void) {
    return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME(NET_MODULE_NAME);
MODULE_DESC("Network stack");
MODULE_FUNCS(net_init, net_unload);
