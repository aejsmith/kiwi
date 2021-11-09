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
 * @brief               Network device control utility.
 */

#include <core/utility.h>

#include <device/net.h>

#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static net_device_t *net_device;

static void usage(void) {
    printf("Usage: net_control command [args...]\n");
    printf("\n");
    printf("command is one of the following:\n\n");
    printf("  up dev_path\n");
    printf("    Brings up the network device at dev_path.\n");
    printf("  down dev_path\n");
    printf("    Shuts down the network device at dev_path.\n");
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

static struct {
    const char *name;
    bool (*func)(int argc, char **argv);
} command_funcs[] = {
    { "up",     command_up },
    { "down",   command_down },
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
