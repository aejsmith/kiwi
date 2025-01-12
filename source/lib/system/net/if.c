/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network interface functions.
 */

#include <net/if.h>

#include "libsystem.h"

void if_freenameindex(struct if_nameindex *ptr) {
    libsystem_stub(__func__, false);
}

char *if_indextoname(unsigned int ifindex, char *ifname) {
    libsystem_stub(__func__, false);
    return NULL;
}

struct if_nameindex *if_nameindex(void) {
    libsystem_stub(__func__, false);
    return NULL;
}

unsigned int if_nametoindex(const char *ifname) {
    libsystem_stub(__func__, false);
    return 0;
}

