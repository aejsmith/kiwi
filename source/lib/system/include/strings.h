/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String functions.
 */

#pragma once

#define __NEED_size_t
#include <bits/alltypes.h>

#include <string.h>

__SYS_EXTERN_C_BEGIN

#define bzero(b, len)       (memset((b), '\0', (len)), (void)0)
#define bcopy(b1, b2, len)  (memmove((b2), (b1), (len)), (void)0)
#define bcmp(b1, b2, len)   memcmp((b1), (b2), (size_t)(len))
#define index(a, b)         strchr((a),(b))
#define rindex(a, b)        strrchr((a),(b))

__SYS_EXTERN_C_END
