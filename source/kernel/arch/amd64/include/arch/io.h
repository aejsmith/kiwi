/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               x86 I/O functions.
 */

#pragma once

#include <types.h>

/**
 * Port I/O functions.
 */

#define ARCH_HAS_PIO 1

typedef uint16_t pio_addr_t;

/** Reads an 8 bit value from a port. */
static inline uint8_t in8(pio_addr_t port) {
    uint8_t ret;

    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Writes an 8 bit value to a port. */
static inline void out8(pio_addr_t port, uint8_t val) {
    __asm__ __volatile__("outb %1, %0" :: "dN"(port), "a"(val));
}

/** Reads a 16 bit value from a port. */
static inline uint16_t in16(pio_addr_t port) {
    uint16_t ret;

    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Writes a 16 bit value to a port. */
static inline void out16(pio_addr_t port, uint16_t val) {
    __asm__ __volatile__("outw %1, %0" :: "dN"(port), "a"(val));
}

/** Reads a 32 bit value from a port. */
static inline uint32_t in32(pio_addr_t port) {
    uint32_t ret;

    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

/** Writes a 32 bit value to a port. */
static inline void out32(pio_addr_t port, uint32_t val) {
    __asm__ __volatile__("outl %1, %0" :: "dN"(port), "a"(val));
}

/** Reads an array of 16 bit values from a port. */
static inline void in16s(pio_addr_t port, size_t count, uint16_t *buf) {
    __asm__ __volatile__(
        "rep insw"
        : "=c"(count), "=D"(buf)
        : "d"(port), "0"(count), "1"(buf)
        : "memory");
}

/** Writes an array of 16 bit values to a port. */
static inline void out16s(pio_addr_t port, size_t count, const uint16_t *buf) {
    __asm__ __volatile__(
        "rep outsw"
        : "=c"(count), "=S"(buf)
        : "d"(port), "0"(count), "1"(buf)
        : "memory");
}

/**
 * Memory mapped I/O functions.
 */

/** Reads an 8 bit value from a memory mapped register. */
static inline uint8_t read8(const volatile uint8_t *addr) {
    uint8_t ret;

    __asm__ __volatile__("movb %1, %0" : "=q"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Writes an 8 bit value to a memory mapped register. */
static inline void write8(volatile uint8_t *addr, uint8_t val) {
    __asm__ __volatile__("movb %0, %1" :: "q"(val), "m"(*addr) : "memory");
}

/** Reads a 16 bit value from a memory mapped register. */
static inline uint16_t read16(const volatile uint16_t *addr) {
    uint16_t ret;

    __asm__ __volatile__("movw %1, %0" : "=r"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Writes a 16 bit value to a memory mapped register. */
static inline void write16(volatile uint16_t *addr, uint16_t val) {
    __asm__ __volatile__("movw %0, %1" :: "r"(val), "m"(*addr) : "memory");
}

/** Reads a 32 bit value from a memory mapped register. */
static inline uint32_t read32(const volatile uint32_t *addr) {
    uint32_t ret;

    __asm__ __volatile__("movl %1, %0" : "=r"(ret) : "m"(*addr) : "memory");
    return ret;
}

/** Writes a 32 bit value to a memory mapped register. */
static inline void write32(volatile uint32_t *addr, uint32_t val) {
    __asm__ __volatile__("movl %0, %1" :: "r"(val), "m"(*addr) : "memory");
}

/** Reads an array of 16 bit values from a memory mapped register. */
static inline void read16s(const volatile uint16_t *addr, size_t count, uint16_t *buf) {
    for (size_t i = 0; i < count; i++)
        buf[i] = read16(addr);
}

/** Writes an array of 16 bit values to a memory mapped register. */
static inline void write16s(volatile uint16_t *addr, size_t count, const uint16_t *buf) {
    for (size_t i = 0; i < count; i++)
        write16(addr, buf[i]);
}
