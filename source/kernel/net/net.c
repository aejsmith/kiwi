/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
#include <net/tcp.h>

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
    net_interface_kdb_init();

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
    tcp_init();

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
