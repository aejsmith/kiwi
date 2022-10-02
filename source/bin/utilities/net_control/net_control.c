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

#include <core/utility.h>

#include <device/net.h>

#include <kernel/device/ipv4_control.h>
#include <kernel/net/ipv4.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static net_device_t *net_device;
static handle_t ipv4_control_device;

static void usage(void) {
    printf("Usage: net_control command [args...]\n");
    printf("\n");
    printf("command is one of the following:\n\n");
    printf("  add_ipv4_addr dev_path addr netmask [broadcast_addr]\n");
    printf("    Adds a new IPv4 address to the network device at dev_path.\n");
    printf("  add_ipv4_route dev_path addr netmask gateway source\n");
    printf("    Adds a new IPv4 routing table entry.\n");
    printf("  down dev_path\n");
    printf("    Shuts down the network device at dev_path.\n");
    printf("  remove_ipv4_addr dev_path addr netmask\n");
    printf("    Removes the specified IPv4 address from the network device at dev_path.\n");
    printf("  remove_ipv4_route dev_path addr netmask gateway source\n");
    printf("    Removes an IPv4 routing table entry.\n");
    printf("  up dev_path\n");
    printf("    Brings up the network device at dev_path.\n");
    printf("\n");
}

static bool open_net_device(const char *path) {
    status_t ret = net_device_open(path, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &net_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to open device '%s': %s\n", path, kern_status_string(ret));
        return false;
    }

    return true;
}

static bool open_ipv4_control_device(void) {
    status_t ret = kern_device_open("/virtual/net/control/ipv4", FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &ipv4_control_device);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "net_control: failed to open IPv4 control device: %s\n", kern_status_string(ret));
        return false;
    }

    return true;
}

static bool parse_ipv4_address(const char *str, net_addr_ipv4_t *addr) {
    uint32_t vals[4];
    int pos = 0;
    int ret = sscanf(str, "%u.%u.%u.%u%n", &vals[0], &vals[1], &vals[2], &vals[3], &pos);
    if (ret != 4 || vals[0] > 255 || vals[1] > 255 || vals[2] > 255 || vals[3] > 255 || str[pos] != 0) {
        fprintf(stderr, "net_control: invalid address '%s'\n", str);
        return false;
    }

    addr->bytes[0] = vals[0];
    addr->bytes[1] = vals[1];
    addr->bytes[2] = vals[2];
    addr->bytes[3] = vals[3];

    return true;
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
        fprintf(stderr, "net_control: failed to bring up '%s': %s\n", path, kern_status_string(ret));
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
        fprintf(stderr, "net_control: failed to shut down '%s': %s\n", path, kern_status_string(ret));
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
        fprintf(stderr, "net_control: failed to add address for '%s': %s\n", path, kern_status_string(ret));
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
        fprintf(stderr, "net_control: failed to remove address for '%s': %s\n", path, kern_status_string(ret));
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
        fprintf(
            stderr, "net_control: failed to get interface ID for '%s': %s\n",
            path, kern_status_string(ret));
        return false;
    }

    ret = kern_file_request(ipv4_control_device, request, &route, sizeof(route), NULL, 0, NULL);
    if (ret != STATUS_SUCCESS) {
        fprintf(
            stderr, "net_control: failed to %s route for '%s': %s\n",
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
};

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return EXIT_SUCCESS;
    }

    for (size_t i = 0; i < core_array_size(command_funcs); i++) {
        if (strcmp(command_funcs[i].name, argv[1]) == 0)
            return command_funcs[i].func(argc - 2, &argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    fprintf(stderr, "Unknown command '%s'\n", argv[1]);
    return EXIT_FAILURE;
}
