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

#include <arpa/inet.h>

#include "net_control.h"

net_device_t *net_device;
handle_t ipv4_control_device;

bool open_net_device(const char *path) {
    status_t ret = net_device_open(path, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &net_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to open device '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    return true;
}

bool open_ipv4_control_device(void) {
    status_t ret = kern_device_open("/virtual/net/control/ipv4", FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &ipv4_control_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to open IPv4 control device: %s\n", kern_status_string(ret));
        return false;
    }

    return true;
}

bool parse_ipv4_address(const char *str, net_addr_ipv4_t *addr) {
    if (inet_pton(AF_INET, str, addr) != 1) {
        fprintf(stderr, "net_control: invalid address '%s'\n", str);
        return false;
    }

    return true;
}
