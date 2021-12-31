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

#include <device/device.h>
#include <device/net/net.h>

#include <io/socket.h>

#include <net/arp.h>
#include <net/ipv4.h>
#include <net/packet.h>

#include <module.h>
#include <status.h>

/** /virtual/net */
device_t *net_virtual_device;

/** /virtual/net/control */
device_t *net_control_device;

static socket_family_t net_socket_families[] = {
    { .id = AF_INET, .create = ipv4_socket_create },
};

static status_t net_init(void) {
    status_t ret;

    net_packet_cache_init();
    net_device_class_init();

    ret = device_create_dir("net", device_virtual_dir, &net_virtual_device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create /virtual/net: %" PRId32, ret);

    device_publish(net_virtual_device);

    ret = device_create_dir("control", net_virtual_device, &net_control_device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to create /virtual/net/control: %" PRId32, ret);

    device_publish(net_control_device);

    arp_init();
    ipv4_init();

    ret = socket_families_register(net_socket_families, array_size(net_socket_families));
    if (ret != STATUS_SUCCESS)
        fatal("Failed to register socket families: %" PRId32, ret);

    return STATUS_SUCCESS;
}

static status_t net_unload(void) {
    return STATUS_NOT_SUPPORTED;
}

MODULE_NAME(NET_MODULE_NAME);
MODULE_DESC("Network stack");
MODULE_FUNCS(net_init, net_unload);
