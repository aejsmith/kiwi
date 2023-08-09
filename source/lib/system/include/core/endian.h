/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Endian conversion functions.
 */

#pragma once

#include <stdint.h>

__SYS_EXTERN_C_BEGIN

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define core_be16_to_cpu(v)  __builtin_bswap16(v)
#   define core_be32_to_cpu(v)  __builtin_bswap32(v)
#   define core_be64_to_cpu(v)  __builtin_bswap64(v)
#   define core_le16_to_cpu(v)  (v)
#   define core_le32_to_cpu(v)  (v)
#   define core_le64_to_cpu(v)  (v)
#   define core_cpu_to_be16(v)  __builtin_bswap16(v)
#   define core_cpu_to_be32(v)  __builtin_bswap32(v)
#   define core_cpu_to_be64(v)  __builtin_bswap64(v)
#   define core_cpu_to_le16(v)  (v)
#   define core_cpu_to_le32(v)  (v)
#   define core_cpu_to_le64(v)  (v)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define core_be16_to_cpu(v)  (v)
#   define core_be32_to_cpu(v)  (v)
#   define core_be64_to_cpu(v)  (v)
#   define core_le16_to_cpu(v)  __builtin_bswap16(v)
#   define core_le32_to_cpu(v)  __builtin_bswap32(v)
#   define core_le64_to_cpu(v)  __builtin_bswap64(v)
#   define core_cpu_to_be16(v)  (v)
#   define core_cpu_to_be32(v)  (v)
#   define core_cpu_to_be64(v)  (v)
#   define core_cpu_to_le16(v)  __builtin_bswap16(v)
#   define core_cpu_to_le32(v)  __builtin_bswap32(v)
#   define core_cpu_to_le64(v)  __builtin_bswap64(v)
#else
#   error "__BYTE_ORDER__ is not defined"
#endif

__SYS_EXTERN_C_END
