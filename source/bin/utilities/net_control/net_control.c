/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network device control utility.
 */

#include <core/utility.h>

#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "net_control.h"

void usage(void) {
    printf(
        "Usage: net_control command [args...]\n"
        "\n"
        "command is one of the following:\n\n"
        "  add_ipv4_addr dev_path addr netmask [broadcast_addr]\n"
        "    Adds a new IPv4 address to the network device at dev_path.\n"
        "  add_ipv4_route dev_path addr netmask gateway source\n"
        "    Adds a new IPv4 routing table entry.\n"
        "  dhcp dev_path\n"
        "    Configure IPv4 address and route via DHCP on the network device at dev_path.\n"
        "  down dev_path\n"
        "    Shuts down the network device at dev_path.\n"
        "  remove_ipv4_addr dev_path addr netmask\n"
        "    Removes the specified IPv4 address from the network device at dev_path.\n"
        "  remove_ipv4_route dev_path addr netmask gateway source\n"
        "    Removes an IPv4 routing table entry.\n"
        "  up dev_path\n"
        "    Brings up the network device at dev_path.\n"
        "\n");
}

static bool command_up(int argc, char **argv) {
    if (argc != 1) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    status_t ret = net_device_up(net_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to bring up '%s': %s", path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool command_down(int argc, char **argv) {
    if (argc != 1) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    status_t ret = net_device_down(net_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to shut down '%s': %s", path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool command_add_ipv4_addr(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    net_interface_addr_ipv4_t addr = {};
    addr.family = AF_INET;

    if (!parse_ipv4_address(argv[1], &addr.addr) || !parse_ipv4_address(argv[2], &addr.netmask))
        return false;

    if (argc > 3) {
        if (!parse_ipv4_address(argv[3], &addr.broadcast))
            return false;
    }

    status_t ret = net_device_add_addr(net_device, &addr, sizeof(addr));
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to add address for '%s': %s", path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool command_remove_ipv4_addr(int argc, char **argv) {
    if (argc != 3) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    net_interface_addr_ipv4_t addr = {};
    addr.family = AF_INET;

    if (!parse_ipv4_address(argv[1], &addr.addr) || !parse_ipv4_address(argv[2], &addr.netmask))
        return false;

    status_t ret = net_device_remove_addr(net_device, &addr, sizeof(addr));
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to remove address for '%s': %s", path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool command_ipv4_route(int argc, char **argv, unsigned request) {
    status_t ret;

    if (argc != 5) {
        usage();
        return false;
    }

    const char *path = argv[0];
    if (!open_net_device(path))
        return false;

    if (!open_ipv4_control_device())
        return false;

    ipv4_route_t route = {};

    if (!parse_ipv4_address(argv[1], &route.addr) ||
        !parse_ipv4_address(argv[2], &route.netmask) ||
        !parse_ipv4_address(argv[3], &route.gateway) ||
        !parse_ipv4_address(argv[4], &route.source))
    {
        return false;
    }

    ret = net_device_interface_id(net_device, &route.interface_id);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to get interface ID for '%s': %s", path, kern_status_string(ret));
        return false;
    }

    ret = kern_file_request(ipv4_control_device, request, &route, sizeof(route), NULL, 0, NULL);
    if (ret != STATUS_SUCCESS) {
        core_log(
            CORE_LOG_ERROR, "failed to %s route for '%s': %s",
            (request == IPV4_CONTROL_DEVICE_REQUEST_ADD_ROUTE) ? "add" : "remove",
            path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool command_add_ipv4_route(int argc, char **argv) {
    return command_ipv4_route(argc, argv, IPV4_CONTROL_DEVICE_REQUEST_ADD_ROUTE);
}

static bool command_remove_ipv4_route(int argc, char **argv) {
    return command_ipv4_route(argc, argv, IPV4_CONTROL_DEVICE_REQUEST_REMOVE_ROUTE);
}

static struct {
    const char *name;
    bool (*func)(int argc, char **argv);
} command_funcs[] = {
    { "up",                 command_up },
    { "down",               command_down },
    { "add_ipv4_addr",      command_add_ipv4_addr },
    { "remove_ipv4_addr",   command_remove_ipv4_addr },
    { "add_ipv4_route",     command_add_ipv4_route },
    { "remove_ipv4_route",  command_remove_ipv4_route },
    { "dhcp",               command_dhcp },
};

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return EXIT_SUCCESS;
    }

    /* Used by DHCP. */
    srand(time(NULL));

    for (size_t i = 0; i < core_array_size(command_funcs); i++) {
        if (strcmp(command_funcs[i].name, argv[1]) == 0)
            return command_funcs[i].func(argc - 2, &argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    core_log(CORE_LOG_ERROR, "unknown command '%s'", argv[1]);
    return EXIT_FAILURE;
}
