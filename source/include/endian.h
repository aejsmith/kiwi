/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Endian conversion functions.
 */

#ifndef __ENDIAN_H
#define __ENDIAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __i386__
# define LITTLE_ENDIAN
#elif defined(__x86_64__)
# define LITTLE_ENDIAN
#endif

/** Swap byte order in a 16-bit value.
 * @param val		Value to swap order of.
 * @return		Converted value. */
static inline uint16_t byte_order_swap16(uint16_t val) {
	uint16_t out = 0;

	out |= (val & 0x00ff) << 8;
	out |= (val & 0xff00) >> 8;
	return out;
}

/** Swap byte order in a 32-bit value.
 * @param val		Value to swap order of.
 * @return		Converted value. */
static inline uint32_t byte_order_swap32(uint32_t val) {
	uint32_t out = 0;

	out |= (val & 0x000000ff) << 24;
	out |= (val & 0x0000ff00) << 8;
	out |= (val & 0x00ff0000) >> 8;
	out |= (val & 0xff000000) >> 24;
	return out;
}

/** Swap byte order in a 64-bit value.
 * @param val		Value to swap order of.
 * @return		Converted value. */
static inline uint64_t byte_order_swap64(uint64_t val) {
	uint64_t out = 0;

	out |= (val & 0x00000000000000ff) << 56;
	out |= (val & 0x000000000000ff00) << 40;
	out |= (val & 0x0000000000ff0000) << 24;
	out |= (val & 0x00000000ff000000) << 8;
	out |= (val & 0x000000ff00000000) >> 8;
	out |= (val & 0x0000ff0000000000) >> 24;
	out |= (val & 0x00ff000000000000) >> 40;
	out |= (val & 0xff00000000000000) >> 56;
	return out;
}

#ifdef LITTLE_ENDIAN
# define be16_to_cpu(v)		byte_order_swap16((v))
# define be32_to_cpu(v)		byte_order_swap32((v))
# define be64_to_cpu(v)		byte_order_swap64((v))
# define le16_to_cpu(v)		(v)
# define le32_to_cpu(v)		(v)
# define le64_to_cpu(v)		(v)
# define cpu_to_be16(v)		byte_order_swap16((v))
# define cpu_to_be32(v)		byte_order_swap32((v))
# define cpu_to_be64(v)		byte_order_swap64((v))
# define cpu_to_le16(v)		(v)
# define cpu_to_le32(v)		(v)
# define cpu_to_le64(v)		(v)
#elif defined(BIG_ENDIAN)
# define be16_to_cpu(v)		(v)
# define be32_to_cpu(v)		(v)
# define be64_to_cpu(v)		(v)
# define le16_to_cpu(v)		byte_order_swap16((v))
# define le32_to_cpu(v)		byte_order_swap32((v))
# define le64_to_cpu(v)		byte_order_swap64((v))
# define cpu_to_be16(v)		(v)
# define cpu_to_be32(v)		(v)
# define cpu_to_be64(v)		(v)
# define cpu_to_le16(v)		byte_order_swap16((v))
# define cpu_to_le32(v)		byte_order_swap32((v))
# define cpu_to_le64(v)		byte_order_swap64((v))
#else
# error "Please define LITTLE_ENDIAN/BIG_ENDIAN for this architecture."
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ENDIAN_H */
