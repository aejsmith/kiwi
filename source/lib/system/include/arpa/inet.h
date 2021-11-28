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
 * @brief               Definitions for internet operations.
 */

#pragma once

#include <core/endian.h>

#include <netinet/in.h>

__SYS_EXTERN_C_BEGIN

#define htonl(val) core_cpu_to_be32(val)
#define htons(val) core_cpu_to_be16(val)
#define ntohl(val) core_be32_to_cpu(val)
#define ntohs(val) core_be16_to_cpu(val)

// TODO. Probably best off taking these (and extra, non-POSIX-standard)
// functions from a BSD.
/* extern in_addr_t inet_addr(const char *str); */
/* char *inet_ntoa(struct in_addr); */
/* const char *inet_ntop(int, const void *__restrict, char *__restrict, socklen_t); */
/* int inet_pton(int, const char *__restrict, void *__restrict); */

__SYS_EXTERN_C_END