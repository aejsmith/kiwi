/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Device I/O functions.
 *
 * These functions are for driver use to perform I/O, either memory-mapped
 * (MMIO) or port-based (PIO), to devices. They internally handle
 * differentiating between MMIO and PIO, allowing drivers to be written without
 * needing to know the type of I/O the device uses on a specific platform.
 */

#pragma once

#include <arch/io.h>

#include <mm/phys.h>

struct device;

/** I/O region handle. */
typedef ptr_t io_region_t;

/** Invalid I/O region value. */
#define IO_REGION_INVALID 0

extern io_region_t mmio_map(phys_ptr_t addr, size_t size, unsigned mmflag);
extern io_region_t device_mmio_map(struct device *device, phys_ptr_t addr, size_t size, unsigned mmflag);

#if ARCH_HAS_PIO
extern io_region_t pio_map(pio_addr_t addr, size_t size);
extern io_region_t device_pio_map(struct device *device, pio_addr_t addr, size_t size);
#endif

extern void io_unmap(io_region_t region, size_t size);

extern uint8_t io_read8(io_region_t region, size_t offset);
extern void io_write8(io_region_t region, size_t offset, uint8_t val);
extern uint16_t io_read16( io_region_t region, size_t offset);
extern void io_write16(io_region_t region, size_t offset, uint16_t val);
extern uint32_t io_read32(io_region_t region, size_t offset);
extern void io_write32(io_region_t region, size_t offset, uint32_t val);
