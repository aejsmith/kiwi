/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Address family definitions.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

typedef uint16_t sa_family_t;

#define AF_UNSPEC               0
#define AF_INET                 1
#define AF_INET6                2
#define AF_UNIX                 3
#define __AF_COUNT              4

#define SOCKADDR_STORAGE_SIZE   128

__KERNEL_EXTERN_C_END
