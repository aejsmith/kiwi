/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               UNIX socket definitions.
 */

#pragma once

#include <kernel/net/family.h>

__KERNEL_EXTERN_C_BEGIN

/** UNIX socket address specification. */
typedef struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[SOCKADDR_STORAGE_SIZE - sizeof(sa_family_t)];
} sockaddr_un_t;

__KERNEL_EXTERN_C_END
