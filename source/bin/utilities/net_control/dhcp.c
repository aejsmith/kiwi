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
 * @brief               Network device control utility.
 */

#include <core/time.h>
#include <core/utility.h>

#include <kernel/object.h>
#include <kernel/socket.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>

#include "dhcp.h"
#include "net_control.h"

#define RETRIES             5
#define MAX_MESSAGE_SIZE    512

static uint8_t hw_addr[NET_DEVICE_ADDR_MAX];
static size_t hw_addr_len;

static handle_t socket_handle;
static sockaddr_in_t broadcast_addr;
static nstime_t abs_timeout;
static uint32_t transaction_id;

static const uint8_t *find_option(const dhcp_header_t *packet, size_t size, uint8_t find) {
    size_t options_size = size - sizeof(dhcp_header_t);
    size_t offset       = 0;

    /* Want at least option and size. */
    while (offset + 2 < options_size) {
        uint8_t option = packet->options[offset];

        if (option == DHCP_OPTION_END)
            break;

        size_t option_size = packet->options[offset + 1];

        if (option == find && offset + option_size + 2 <= options_size)
            return &packet->options[offset];

        offset += option_size + 2;
    }

    return NULL;
}

static bool send_discover(void) {
    status_t ret;

    uint8_t options[] = {
        DHCP_OPTION_MESSAGE_TYPE,
        1, /* length */
        DHCP_MESSAGE_DHCPDISCOVER,

        DHCP_OPTION_PARAM_REQUEST,
        2,
        DHCP_OPTION_SUBNET_MASK,
        DHCP_OPTION_ROUTER,
        // TODO: Request DNS

        DHCP_OPTION_END,
        0
    };

    size_t size = sizeof(dhcp_header_t) + sizeof(options);
    dhcp_header_t *packet = calloc(1, size);
    packet->op    = DHCP_OP_BOOTREQUEST;
    packet->htype = 1;
    packet->hlen  = 6;
    packet->xid   = htonl(transaction_id);
    packet->magic = htonl(DHCP_MAGIC);

    memcpy(packet->chaddr, hw_addr, hw_addr_len);
    memcpy(packet->options, options, sizeof(options));

    ret = kern_socket_sendto(
        socket_handle, packet, size, 0, (sockaddr_t *)&broadcast_addr,
        sizeof(broadcast_addr), NULL);

    free(packet);

    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to send DHCPDISCOVER: %s\n", kern_status_string(ret));
        return false;
    }

    return true;
}

static status_t wait_message(uint8_t type, dhcp_header_t **_packet, size_t *_size) {
    status_t ret;

    /* Wait for response. */
    dhcp_header_t *packet = calloc(1, MAX_MESSAGE_SIZE);
    size_t size;
    while (true) {
        nstime_t curr_time;
        kern_time_get(TIME_SYSTEM, &curr_time);
        nstime_t timeout = abs_timeout - core_min(curr_time, abs_timeout);

        object_event_t event = {};
        event.handle = socket_handle;
        event.event  = FILE_EVENT_READABLE;

        ret = kern_object_wait(&event, 1, 0, timeout);
        if (ret != STATUS_SUCCESS) {
            if (ret != STATUS_TIMED_OUT)
                fprintf(stderr, "net_control: failed to wait for message: %s\n", kern_status_string(ret));

            free(packet);
            return ret;
        }

        ret = kern_socket_recvfrom(socket_handle, packet, MAX_MESSAGE_SIZE, 0, 0, &size, NULL, NULL);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "net_control: failed to receive message: %s\n", kern_status_string(ret));

            free(packet);
            return ret;
        }

        /* Check if this is valid and looks like what we want. */
        if (size >= sizeof(dhcp_header_t) &&
            ntohl(packet->magic) == DHCP_MAGIC &&
            packet->op == DHCP_OP_BOOTREPLY &&
            ntohl(packet->xid) == transaction_id)
        {
            const uint8_t *option = find_option(packet, size, DHCP_OPTION_MESSAGE_TYPE);
            if (option && option[2] == type)
                break;
        }
    }

    *_packet = packet;
    *_size   = size;
    return STATUS_SUCCESS;
}

bool command_dhcp(int argc, char **argv) {
    status_t ret;

    if (argc != 1) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    /* Bring it down in case it's up to clear any existing configuration. */
    ret = net_device_down(net_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to shut down '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    ret = net_device_up(net_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to bring up '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    ret = net_device_hw_addr(net_device, hw_addr, &hw_addr_len);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to get HW address for '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    uint32_t interface_id;
    ret = net_device_interface_id(net_device, &interface_id);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to get interface ID for '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    ret = kern_socket_create(AF_INET, SOCK_DGRAM, 0, 0, &socket_handle);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to create socket: %s\n", kern_status_string(ret));
        return false;
    }

    /* Bind specifically to this interface. This allows us to broadcast on it. */
    ret = kern_socket_setsockopt(socket_handle, SOL_SOCKET, SO_BINDTOINTERFACE, &interface_id, sizeof(interface_id));
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to bind socket to interface: %s\n", kern_status_string(ret));
        return false;
    }

    sockaddr_in_t client_addr = {};
    client_addr.sin_family      = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port        = htons(DHCP_CLIENT_PORT);

    ret = kern_socket_bind(socket_handle, (sockaddr_t *)&client_addr, sizeof(client_addr));
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to bind port: %s\n", kern_status_string(ret));
        return false;
    }

    broadcast_addr.sin_family      = AF_INET;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcast_addr.sin_port        = htons(DHCP_SERVER_PORT);

    nstime_t next_timeout = core_secs_to_nsecs(4);

    for (size_t i = 0; i < RETRIES; i++) {
        nstime_t curr_time;
        kern_time_get(TIME_SYSTEM, &curr_time);
        abs_timeout = curr_time + next_timeout;

        /* Exponential backoff, per DHCP RFC. */
        next_timeout *= 2;

        /* Allocate random transaction ID. */
        transaction_id = rand();

        /* Send DHCPDISCOVER. */
        if (!send_discover())
            return false;

        /* Wait for an offer. */
        dhcp_header_t *offer;
        size_t offer_size;
        ret = wait_message(DHCP_MESSAGE_DHCPOFFER, &offer, &offer_size);
        if (ret != STATUS_SUCCESS) {
            if (ret == STATUS_TIMED_OUT) {
                continue;
            } else {
                return false;
            }
        }

        printf("Got offer\n");
    }

    return true;
}
