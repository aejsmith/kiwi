/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Definitions for internet operations.
 */

#pragma once

#include <netinet/in.h>

#include <features.h>

__SYS_EXTERN_C_BEGIN

extern in_addr_t inet_addr(const char *p);
extern char *inet_ntoa(struct in_addr in);
extern int inet_aton(const char *s0, struct in_addr *dest);
extern const char *inet_ntop(int af, const void *__restrict a0, char *__restrict s, socklen_t len);
extern int inet_pton(int af, const char *__restrict s, void *__restrict a0);

__SYS_EXTERN_C_END
