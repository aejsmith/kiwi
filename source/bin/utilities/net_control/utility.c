/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network device control utility.
 */

#include <arpa/inet.h>

#include "net_control.h"

net_device_t *net_device;
const char *net_device_path;
handle_t ipv4_control_device = INVALID_HANDLE;

bool open_net_device(const char *path) {
    net_device_path = path;

    status_t ret = net_device_open(path, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &net_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open device '%s': %s", path, kern_status_string(ret));
        return false;
    }

    return true;
}

bool open_ipv4_control_device(void) {
    status_t ret = kern_device_open("/virtual/net/control/ipv4", FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &ipv4_control_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open IPv4 control device: %s", kern_status_string(ret));
        return false;
    }

    return true;
}

bool parse_ipv4_address(const char *str, net_addr_ipv4_t *addr) {
    if (inet_pton(AF_INET, str, addr) != 1) {
        core_log(CORE_LOG_ERROR, "invalid address '%s'", str);
        return false;
    }

    return true;
}
