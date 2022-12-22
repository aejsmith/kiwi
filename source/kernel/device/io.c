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
 * @brief               Device I/O functions.
 */

#include <device/device.h>
#include <device/io.h>

#include <mm/mmu.h>

#include <assert.h>

typedef struct device_io_resource {
    io_region_t region;
    size_t size;
} device_io_resource_t;

#if ARCH_HAS_PIO

/* Port addresses are offset in the handle since they can start from 0, and we
 * want to reserve 0 for IO_REGION_INVALID/NULL. */
#define PIO_OFFSET 0x10000
#define PIO_MASK   0x0ffff
#define PIO_END    0x20000

#define do_io(mmio, pio) \
    assert(region >= PIO_OFFSET); \
    if (region >= PIO_END) { \
        mmio; \
    } else { \
        region &= PIO_MASK; \
        pio; \
    }

#else

#define do_io(mmio, pio) mmio

#endif

#define MMIO_MAP_MMU_FLAGS (MMU_ACCESS_RW | MMU_CACHE_DEVICE)

/**
 * Maps physical memory for memory-mapped I/O. The returned handle can be used
 * with io_{read,write}* functions to perform I/O (it must not be used
 * directly).
 *
 * This function is a shorthand which maps the memory as
 * (MMU_ACCESS_RW | MMU_CACHE_DEVICE), which is appropriate for most device
 * memory mappings. Use mmio_map_etc() if other flags are desired.
 *
 * @param addr          Physical address to map.
 * @param size          Size of address to map.
 * @param mmflag        Memory allocation flags.
 *
 * @return              I/O region handle, or IO_REGION_INVALID on failure.
 */
io_region_t mmio_map(phys_ptr_t addr, size_t size, unsigned mmflag) {
    return mmio_map_etc(addr, size, MMIO_MAP_MMU_FLAGS, mmflag);
}

/**
 * Maps physical memory for memory-mapped I/O. The returned handle can be used
 * with io_{read,write}* functions to perform I/O (it must not be used
 * directly).
 *
 * @param addr          Physical address to map.
 * @param size          Size of address to map.
 * @param flags         MMU mapping flags.
 * @param mmflag        Memory allocation flags.
 *
 * @return              I/O region handle, or IO_REGION_INVALID on failure.
 */
io_region_t mmio_map_etc(phys_ptr_t addr, size_t size, uint32_t flags, unsigned mmflag) {
    assert(size > 0);

    return (io_region_t)phys_map_etc(addr, size, flags, mmflag);
}

/**
 * Maps physical memory for memory-mapped I/O, as a device-managed resource
 * (will be unmapped when the device is destroyed).
 *
 * @see                 mmio_map().
 *
 * @param device        Device to register to.
 */
io_region_t device_mmio_map(device_t *device, phys_ptr_t addr, size_t size, unsigned mmflag) {
    return device_mmio_map_etc(device, addr, size, MMIO_MAP_MMU_FLAGS, mmflag);
}

/**
 * Maps physical memory for memory-mapped I/O, as a device-managed resource
 * (will be unmapped when the device is destroyed).
 *
 * @see                 mmio_map().
 *
 * @param device        Device to register to.
 */
io_region_t device_mmio_map_etc(
    device_t *device, phys_ptr_t addr, size_t size, uint32_t flags,
    unsigned mmflag)
{
    return (io_region_t)device_phys_map_etc(device, addr, size, flags, mmflag);
}

#if ARCH_HAS_PIO

/**
 * Maps a port range for port-based I/O. The returned handle can be used with
 * io_{read,write}* functions to perform I/O (it must not be used directly).
 *
 * @param addr          Port address to map.
 * @param size          Size of address range to map.
 * @param mmflag        Memory allocation flags.
 *
 * @return              I/O region handle, or IO_REGION_INVALID on failure.
 */
io_region_t pio_map(pio_addr_t addr, size_t size) {
    assert(size > 0);
    assert(addr + size <= (PIO_END - PIO_OFFSET));

    return (io_region_t)addr + PIO_OFFSET;
}

/**
 * Maps a port range for port-based I/O, as a device-managed resource (will be
 * unmapped when the device is destroyed).
 *
 * @see                 pio_map().
 *
 * @param device        Device to register to.
 */
io_region_t device_pio_map(struct device *device, pio_addr_t addr, size_t size) {
    /* Currently, there's nothing to unmap, so no tracking needed. We have this
     * API in case we do have need for it in future, for example we might add
     * exclusive ownership of PIO/MMIO regions. */
    return pio_map(addr, size);
}

/** Return whether an I/O region is PIO. */
bool io_is_pio(io_region_t region) {
    do_io(
        return false,
        return true
    );
}

/** Return the address of an I/O region. */
ptr_t io_addr(io_region_t region) {
    do_io(
        return region,
        return region
    );
}

#endif

/** Unmap an I/O region.
 * @param region        Region to unmap.
 * @param size          Size of region. */
void io_unmap(io_region_t region, size_t size) {
    do_io(
        phys_unmap((void *)region, size),
        /* Nothing. */
    );
}

/** Reads an 8 bit value from an I/O region. */
uint8_t io_read8(io_region_t region, size_t offset) {
    do_io(
        return read8((const volatile uint8_t *)(region + offset)),
        return in8((pio_addr_t)(region + offset))
    );
}

/** Writes an 8 bit value to an I/O region. */
void io_write8(io_region_t region, size_t offset, uint8_t val) {
    do_io(
        write8((volatile uint8_t *)(region + offset), val),
        out8((pio_addr_t)(region + offset), val)
    );
}

/** Reads a 16 bit value from an I/O region. */
uint16_t io_read16( io_region_t region, size_t offset) {
    do_io(
        return read16((const volatile uint16_t *)(region + offset)),
        return in16((pio_addr_t)(region + offset))
    );
}

/** Writes a 16 bit value to an I/O region. */
void io_write16(io_region_t region, size_t offset, uint16_t val) {
    do_io(
        write16((volatile uint16_t *)(region + offset), val),
        out16((pio_addr_t)(region + offset), val)
    );
}

/** Reads a 32 bit value from an I/O region. */
uint32_t io_read32(io_region_t region, size_t offset) {
    do_io(
        return read32((const volatile uint32_t *)(region + offset)),
        return in32((pio_addr_t)(region + offset))
    );
}

/** Writes a 32 bit value to an I/O region. */
void io_write32(io_region_t region, size_t offset, uint32_t val) {
    do_io(
        write32((volatile uint32_t *)(region + offset), val),
        out32((pio_addr_t)(region + offset), val)
    );
}

/** Reads an array of 16 bit values from a port in an I/O region. */
void io_read16s(io_region_t region, size_t offset, size_t count, uint16_t *buf) {
    do_io(
        read16s((const volatile uint16_t *)(region + offset), count, buf),
        in16s((pio_addr_t)(region + offset), count, buf)
    );
}

/** Writes an array of 16 bit values to a port in an I/O region. */
void io_write16s(io_region_t region, size_t offset, size_t count, const uint16_t *buf) {
    do_io(
        write16s((volatile uint16_t *)(region + offset), count, buf),
        out16s((pio_addr_t)(region + offset), count, buf)
    );
}
