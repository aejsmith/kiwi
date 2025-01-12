/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network interface functions.
 */

#pragma once

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

#define IF_NAMESIZE 16

struct if_nameindex {
    unsigned int if_index;
    char *if_name;
};

extern void if_freenameindex(struct if_nameindex *ptr);
extern char *if_indextoname(unsigned int ifindex, char *ifname);
extern struct if_nameindex *if_nameindex(void);
extern unsigned int if_nametoindex(const char *ifname);

__SYS_EXTERN_C_END
