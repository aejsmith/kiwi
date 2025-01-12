/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
