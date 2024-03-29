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
 * @brief               ARM64 I/O functions.
 */

#pragma once

#include <types.h>

/**
 * Memory mapped I/O functions.
 */

/** Read an 8 bit value from a memory mapped register. */
static inline uint8_t read8(const volatile uint8_t *addr) {
    uint8_t ret;

    __asm__ __volatile__("ldrb %w0, [%1]" : "=r"(ret) : "r"(addr));
    return ret;
}

/** Write an 8 bit value to a memory mapped register. */
static inline void write8(volatile uint8_t *addr, uint8_t val) {
    __asm__ __volatile__("strb %w0, [%1]" :: "rZ"(val), "r"(addr));
}

/** Read a 16 bit value from a memory mapped register. */
static inline uint16_t read16(const volatile uint16_t *addr) {
    uint16_t ret;

    __asm__ __volatile__("ldrh %w0, [%1]" : "=r"(ret) : "r"(addr));
    return ret;
}

/** Write a 16 bit value to a memory mapped register. */
static inline void write16(volatile uint16_t *addr, uint16_t val) {
    __asm__ __volatile__("strh %w0, [%1]" :: "rZ"(val), "r"(addr));
}

/** Read a 32 bit value from a memory mapped register. */
static inline uint32_t read32(const volatile uint32_t *addr) {
    uint32_t ret;

    __asm__ __volatile__("ldr %w0, [%1]" : "=r"(ret) : "r"(addr));
    return ret;
}

/** Write a 32 bit value to a memory mapped register. */
static inline void write32(volatile uint32_t *addr, uint32_t val) {
    __asm__ __volatile__("str %w0, [%1]" :: "rZ"(val), "r"(addr));
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
