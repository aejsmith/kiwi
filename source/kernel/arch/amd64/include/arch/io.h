/*
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		x86 port I/O functions.
 */

#ifndef __ARCH_IO_H
#define __ARCH_IO_H

#include <types.h>

/** Read 8 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint8_t in8(uint16_t port) {
	uint8_t rv;

	__asm__ volatile("inb %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 8 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out8(uint16_t port, uint8_t data) {
	__asm__ volatile("outb %1, %0" : : "dN"(port), "a"(data));
}

/** Read 16 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint16_t in16(uint16_t port) {
	uint16_t rv;

	__asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 16 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out16(uint16_t port, uint16_t data) {
	__asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

/** Read 32 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint32_t in32(uint16_t port) {
	uint32_t rv;

	__asm__ volatile("inl %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 32 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out32(uint16_t port, uint32_t data) {
	__asm__ volatile("outl %1, %0" : : "dN"(port), "a"(data));
}

/** Write an array of 16 bits to a port.
 * @param port		Port to write to.
 * @param count		Number of 16 byte values to write.
 * @param buf		Buffer to write from. */
static inline void out16s(uint16_t port, size_t count, const uint16_t *buf) {
	__asm__ volatile("rep outsw" : "=c"(count), "=S"(buf) : "d"(port), "0"(count), "1"(buf) : "memory");
}

/** Read an array of 16 bits from a port.
 * @param port		Port to read from.
 * @param count		Number of 16 byte values to read.
 * @param buf		Buffer to read into. */
static inline void in16s(uint16_t port, size_t count, uint16_t *buf) {
	__asm__ volatile("rep insw" : "=c"(count), "=D"(buf) : "d"(port), "0"(count), "1"(buf) : "memory");
}

#endif /* __ARCH_IO_H */
