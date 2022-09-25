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

