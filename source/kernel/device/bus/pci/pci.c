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
 * @brief               PCI bus manager.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

#include "pci.h"

/** PCI device bus. */
__export bus_t pci_bus;

static SPINLOCK_DEFINE(pci_config_lock);

/** Read an 8-bit value from a PCI device's configuration space.
 * @param device        Device to read from.
 * @param reg           Offset to read from.
 * @return              Value read. */
__export uint8_t pci_config_read8(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint8_t ret = platform_pci_config_read8(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write an 8-bit value to a PCI device's configuration space.
 * @param device        Device to write to.
 * @param reg           Offset to write to.
 * @param val           Value to write. */
__export void pci_config_write8(pci_device_t *device, uint8_t reg, uint8_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write8(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

/** Read a 16-bit value from a PCI device's configuration space.
 * @param device        Device to read from.
 * @param reg           Offset to read from.
 * @return              Value read. */
__export uint16_t pci_config_read16(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint16_t ret = platform_pci_config_read16(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write a 16-bit value to a PCI device's configuration space.
 * @param device        Device to write to.
 * @param reg           Offset to write to.
 * @param val           Value to write. */
__export void pci_config_write16(pci_device_t *device, uint8_t reg, uint16_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write16(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

/** Read a 32-bit value from a PCI device's configuration space.
 * @param device        Device to read from.
 * @param reg           Offset to read from.
 * @return              Value read. */
__export uint32_t pci_config_read32(pci_device_t *device, uint8_t reg) {
    spinlock_lock(&pci_config_lock);
    uint32_t ret = platform_pci_config_read32(&device->addr, reg);
    spinlock_unlock(&pci_config_lock);
    return ret;
}

/** Write a 32-bit value to a PCI device's configuration space.
 * @param device        Device to write to.
 * @param reg           Offset to write to.
 * @param val           Value to write. */
__export void pci_config_write32(pci_device_t *device, uint8_t reg, uint32_t val) {
    spinlock_lock(&pci_config_lock);
    platform_pci_config_write32(&device->addr, reg, val);
    spinlock_unlock(&pci_config_lock);
}

static status_t get_map_params(
    pci_device_t *device, uint8_t index, phys_ptr_t *_base, phys_size_t *_size,
    uint32_t *_flags)
{
    assert(index < PCI_MAX_BARS);

    pci_bar_t *bar = &device->bars[index];

    /* Check if there is a BAR here. */
    if (bar->size == 0)
        return STATUS_NOT_FOUND;

    phys_ptr_t offset = *_base;
    phys_size_t size  = *_size;
    uint32_t flags    = *_flags;

    /* Validate offset and size. */
    if (offset >= bar->size)
        return STATUS_INVALID_ADDR;
    if (size == 0)
        size = bar->size - offset;
    if (offset + size > bar->size)
        return STATUS_INVALID_ADDR;

    if (!bar->is_pio) {
        uint32_t cache_mode = flags & MMU_CACHE_MASK;
        assert(cache_mode == 0 || cache_mode == MMU_CACHE_WRITE_COMBINE);

        /* Set cache mode according to prefetchable flag and whether WC is
         * requested. */
        if (bar->prefetchable) {
            if (cache_mode != MMU_CACHE_WRITE_COMBINE)
                cache_mode = MMU_CACHE_NORMAL;
        } else {
            cache_mode = MMU_CACHE_DEVICE;
        }

        flags = (flags & ~MMU_CACHE_MASK) | cache_mode;
    }

    *_base  = bar->base + offset;
    *_size  = size;
    *_flags = flags;
    return STATUS_SUCCESS;
}

/**
 * Maps a PCI device BAR. This will return an I/O region handle to the BAR
 * mapping using the correct type of I/O for the BAR.
 *
 * For memory-mapped BARs, the mapping will be created with MMU_ACCESS_RW,
 * and either MMU_CACHE_NORMAL if the BAR is prefetchable or MMU_CACHE_DEVICE
 * otherwise. Use pci_bar_map_etc() to change this.
 *
 * The full detected range of the BAR is mapped. To map only a sub-range, use
 * pci_bar_map_etc().
 *
 * The region should be unmapped with pci_bar_unmap(), which will handle
 * passing the correct size to io_unmap().
 *
 * @param device        Device to map for.
 * @param index         Index of the BAR to map.
 * @param mmflag        Allocation behaviour flags.
 * @param _region       Where to store I/O region handle.
 *
 * @return              STATUS_NOT_FOUND if BAR does not exist.
 *                      STATUS_NO_MEMORY if mapping the BAR failed.
 */
__export status_t pci_bar_map(pci_device_t *device, uint8_t index, unsigned mmflag, io_region_t *_region) {
    return pci_bar_map_etc(device, index, 0, 0, MMU_ACCESS_RW, mmflag, _region);
}

/**
 * Maps a PCI device BAR. This will return an I/O region handle to the BAR
 * mapping using the correct type of I/O for the BAR.
 *
 * For memory-mapped BARs, the mapping will be created with the specified
 * access. The cache mode will be set according to the BAR prefetchable flags
 * as with pci_bar_map(), with the exception that MMU_CACHE_WRITE_COMBINE is
 * accepted, which will be used over MMU_CACHE_NORMAL if the BAR is
 * prefetchable. Any other cache flags are not allowed.
 *
 * This allows only a sub-range of the BAR to be mapped. An error will be
 * returned if the specified range goes outside of the maximum BAR range.
 *
 * The region should be unmapped with pci_bar_unmap_etc(), which will handle
 * passing the correct size to io_unmap(). pci_bar_unmap() can be used if the
 * offset and size are both 0.
 *
 * @param device        Device to map for.
 * @param index         Index of the BAR to map.
 * @param offset        Offset into the BAR to map from.
 * @param size          Size of the range to map (if 0, will map the whole BAR
 *                      from the offset).
 * @param flags         MMU mapping flags.
 * @param mmflag        Allocation behaviour flags.
 * @param _region       Where to store I/O region handle.
 *
 * @return              STATUS_NOT_FOUND if BAR does not exist.
 *                      STATUS_INVALID_ADDR if range is outside of the BAR.
 *                      STATUS_NO_MEMORY if mapping the BAR failed.
 */
__export status_t pci_bar_map_etc(
    pci_device_t *device, uint8_t index, phys_ptr_t offset, phys_size_t size,
    uint32_t flags, unsigned mmflag, io_region_t *_region)
{
    phys_ptr_t base = offset;
    status_t ret = get_map_params(device, index, &base, &size, &flags);
    if (ret != STATUS_SUCCESS)
        return ret;

    io_region_t region;
    if (device->bars[index].is_pio) {
        #if ARCH_HAS_PIO
            region = pio_map(base, size);
            assert(region != IO_REGION_INVALID);
        #else
            unreachable();
        #endif
    } else {
        region = mmio_map_etc(base, size, flags, mmflag);
        if (region == IO_REGION_INVALID)
            return STATUS_NO_MEMORY;
    }

    *_region = region;
    return STATUS_SUCCESS;
}

/** Unmaps a previously mapped BAR from pci_bar_map().
 * @param device        Device to unmap for.
 * @param index         Index of the BAR to unmap.
 * @param region        I/O region handle to unmap. */
__export void pci_bar_unmap(pci_device_t *device, uint8_t index, io_region_t region) {
    pci_bar_unmap_etc(device, index, region, 0, 0);
}

/** Unmaps a previously mapped BAR sub-range from pci_bar_map_etc().
 * @param device        Device to unmap for.
 * @param index         Index of the BAR to unmap.
 * @param region        I/O region handle to unmap.
 * @param offset        Offset into the BAR that was mapped.
 * @param size          Size of the range that was mapped. */
__export void pci_bar_unmap_etc(
    pci_device_t *device, uint8_t index, io_region_t region, phys_ptr_t offset,
    phys_size_t size)
{
    assert(index < PCI_MAX_BARS);

    pci_bar_t *bar = &device->bars[index];

    assert(bar->size != 0);
    assert(offset < bar->size);

    if (size == 0)
        size = bar->size - offset;

    assert(offset + size <= bar->size);

    io_unmap(region, size);
}

/**
 * Maps a PCI device BAR, as a device-managed resource (will be unmapped when
 * the device is destroyed).
 *
 * @see                 pci_bar_map().
 *
 * @param owner         Device to register to.
 */
__export status_t device_pci_bar_map(
    device_t *owner, pci_device_t *device, uint8_t index, unsigned mmflag,
    io_region_t *_region)
{
    return device_pci_bar_map_etc(owner, device, index, 0, 0, MMU_ACCESS_RW, mmflag, _region);
}

/**
 * Maps a PCI device BAR, as a device-managed resource (will be unmapped when
 * the device is destroyed).
 *
 * @see                 pci_bar_map().
 *
 * @param owner         Device to register to.
 */
__export status_t device_pci_bar_map_etc(
    device_t *owner, pci_device_t *device, uint8_t index, phys_ptr_t offset,
    phys_size_t size, uint32_t flags, unsigned mmflag, io_region_t *_region)
{
    phys_ptr_t base = offset;
    status_t ret = get_map_params(device, index, &base, &size, &flags);
    if (ret != STATUS_SUCCESS)
        return ret;

    io_region_t region;
    if (device->bars[index].is_pio) {
        #if ARCH_HAS_PIO
            region = device_pio_map(owner, base, size);
            assert(region != IO_REGION_INVALID);
        #else
            unreachable();
        #endif
    } else {
        region = device_mmio_map_etc(owner, base, size, flags, mmflag);
        if (region == IO_REGION_INVALID)
            return STATUS_NO_MEMORY;
    }

    *_region = region;
    return STATUS_SUCCESS;
}

static void make_device_name(pci_address_t *addr, char *str) {
    snprintf(
        str, PCI_NAME_MAX, "%04x:%02x:%02x.%x",
        addr->domain, addr->bus, addr->dev, addr->func);
}

static void scan_bars(pci_device_t *device) {
    memset(device->bars, 0, sizeof(device->bars));

    for (size_t i = 0; i < PCI_MAX_BARS; i++) {
        uint8_t reg  = PCI_CONFIG_BAR0 + (i * 4);
        uint64_t bar = platform_pci_config_read32(&device->addr, reg);

        if (bar == 0)
            continue;

        device->bars[i].is_pio = bar & (1 << 0);

        uint8_t width;
        uint64_t mask;
        if (device->bars[i].is_pio) {
            /* I/O space. */
            #if ARCH_HAS_PIO
                width = 32;
                mask  = 0xfffffffcul;
            #else
                kprintf(LOG_WARN, "pci:   BAR %zu is PIO but PIO is unsupported, ignoring...\n");
                continue;
            #endif
        } else {
            /* Memory space. */
            uint32_t type = (bar >> 1) & 3;
            if (type == 0) {
                width = 32;
                mask  = 0xfffffff0ul;
            } else if (type == 2) {
                width = 64;
                mask  = 0xfffffffffffffff0ul;
            } else {
                kprintf(LOG_WARN, "pci:   BAR %zu has unrecognized memory type, ignoring...\n");
                continue;
            }

            device->bars[i].prefetchable = bar & (1 << 3);
        }

        /* Determine BAR size by writing all 1s to the BAR, reading it back and
         * decoding, then set it back to the original value. */
        platform_pci_config_write32(&device->addr, reg, 0xffffffff);
        uint64_t size = platform_pci_config_read32(&device->addr, reg);
        platform_pci_config_write32(&device->addr, reg, (uint32_t)bar);

        if (width == 64) {
            uint32_t bar_hi = platform_pci_config_read32(&device->addr, reg + 4);

            platform_pci_config_write32(&device->addr, reg + 4, 0xffffffff);
            uint32_t size_hi = platform_pci_config_read32(&device->addr, reg + 4);
            platform_pci_config_write32(&device->addr, reg + 4, bar_hi);

            bar  |= (uint64_t)bar_hi << 32;
            size |= (uint64_t)size_hi << 32;
        }

        device->bars[i].base = bar & mask;
        device->bars[i].size = (~(size & mask) + 1) & mask;

        if (device->bars[i].is_pio) {
            kprintf(
                LOG_NOTICE, "pci:   BAR %zu PIO @ 0x%" PRIxPHYS " size 0x%" PRIxPHYS "\n",
                i, device->bars[i].base, device->bars[i].size);
        } else {
            kprintf(
                LOG_NOTICE, "pci:   BAR %zu MMIO @ 0x%" PRIxPHYS " size 0x%" PRIxPHYS " (%" PRIu8 "-bit%s)\n",
                i, device->bars[i].base, device->bars[i].size,
                width, (device->bars[i].prefetchable) ? ", prefetchable" : "");
        }
    }
}

static pci_device_t *scan_device(pci_address_t *addr) {
    /* Check for device presence. */
    spinlock_lock(&pci_config_lock);
    uint16_t vendor_id = platform_pci_config_read16(addr, PCI_CONFIG_VENDOR_ID);
    spinlock_unlock(&pci_config_lock);

    if (vendor_id == 0xffff)
        return NULL;

    pci_device_t *device = kmalloc(sizeof(pci_device_t), MM_KERNEL);

    bus_device_init(&device->bus);

    device->addr = *addr;

    spinlock_lock(&pci_config_lock);

    /* Retrieve common configuration information. */
    device->device_id      = platform_pci_config_read16(addr, PCI_CONFIG_DEVICE_ID);
    device->vendor_id      = vendor_id;
    device->base_class     = platform_pci_config_read8(addr, PCI_CONFIG_BASE_CLASS);
    device->sub_class      = platform_pci_config_read8(addr, PCI_CONFIG_SUB_CLASS);
    device->prog_iface     = platform_pci_config_read8(addr, PCI_CONFIG_PI);
    device->revision       = platform_pci_config_read8(addr, PCI_CONFIG_REVISION);
    device->header_type    = platform_pci_config_read8(addr, PCI_CONFIG_HEADER_TYPE);
    device->interrupt_line = platform_pci_config_read8(addr, PCI_CONFIG_INTERRUPT_LINE);
    device->interrupt_pin  = platform_pci_config_read8(addr, PCI_CONFIG_INTERRUPT_PIN);

    char name[PCI_NAME_MAX];
    make_device_name(addr, name);

    kprintf(
        LOG_NOTICE, "pci: device %s ID %04x:%04x class %02x%02x\n",
        name, device->vendor_id, device->device_id, device->base_class,
        device->sub_class);

    /* Get BAR information. */
    scan_bars(device);

    spinlock_unlock(&pci_config_lock);

    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS,          DEVICE_ATTR_STRING, { .string = PCI_DEVICE_CLASS_NAME } },
        { PCI_DEVICE_ATTR_VENDOR_ID,  DEVICE_ATTR_UINT16, { .uint16 = device->vendor_id     } },
        { PCI_DEVICE_ATTR_DEVICE_ID,  DEVICE_ATTR_UINT16, { .uint16 = device->device_id     } },
        { PCI_DEVICE_ATTR_BASE_CLASS, DEVICE_ATTR_UINT8,  { .uint8  = device->base_class    } },
        { PCI_DEVICE_ATTR_SUB_CLASS,  DEVICE_ATTR_UINT8,  { .uint8  = device->sub_class     } },
    };

    status_t ret = bus_create_device(&pci_bus, &device->bus, name, NULL, attrs, array_size(attrs));
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "pci: failed to create device %s: %" PRId32, name, ret);
        kfree(device);
        return NULL;
    }

    /* Check for a PCI-to-PCI bridge. */
    if (device->base_class == 0x06 && device->sub_class == 0x04) {
        spinlock_lock(&pci_config_lock);
        uint8_t dest = platform_pci_config_read8(addr, PCI_CONFIG_P2P_SUBORDINATE_BUS);
        spinlock_unlock(&pci_config_lock);

        kprintf(LOG_NOTICE, "pci: PCI-to-PCI bridge to %02x\n", dest);
        pci_scan_bus(addr->domain, dest);
    }

    return device;
}

/** Scan for devices on a bus. */
void pci_scan_bus(uint16_t domain, uint8_t bus) {
    kprintf(LOG_NOTICE, "pci: scanning bus %04x:%02x\n", domain, bus);

    pci_address_t addr;
    addr.domain = domain;
    addr.bus    = bus;

    for (addr.dev = 0; addr.dev < 32; addr.dev++) {
        addr.func = 0;

        pci_device_t *device = scan_device(&addr);

        if (device && device->header_type & 0x80) {
            /* Multifunction device. */
            for (addr.func = 1; addr.func < 8; addr.func++)
                scan_device(&addr);
        }
    }
}

/** Match a PCI device to a driver. */
static bool pci_bus_match_device(bus_device_t *_device, bus_driver_t *_driver) {
    pci_device_t *device = container_of(_device, pci_device_t, bus);
    pci_driver_t *driver = container_of(_driver, pci_driver_t, bus);

    for (size_t i = 0; i < driver->matches.count; i++) {
        pci_match_t *match = &driver->matches.array[i];

        bool matches = true;

        if (match->vendor_id != PCI_MATCH_ANY_ID)
            matches &= device->vendor_id == match->vendor_id;

        if (match->device_id != PCI_MATCH_ANY_ID)
            matches &= device->device_id == match->device_id;

        if (match->base_class != PCI_MATCH_ANY_ID)
            matches &= device->base_class == match->base_class;

        if (match->sub_class != PCI_MATCH_ANY_ID)
            matches &= device->sub_class == match->sub_class;

        if (matches) {
            device->match = match;
            return true;
        }
    }

    return false;
}

/** Initialize a PCI device. */
static status_t pci_bus_init_device(bus_device_t *_device, bus_driver_t *_driver) {
    pci_device_t *device = container_of(_device, pci_device_t, bus);
    pci_driver_t *driver = container_of(_driver, pci_driver_t, bus);

    return driver->init_device(device);
}

static bus_type_t pci_bus_type = {
    .name         = "pci",
    .device_class = PCI_DEVICE_CLASS_NAME,
    .match_device = pci_bus_match_device,
    .init_device  = pci_bus_init_device,
};

static status_t pci_init(void) {
    status_t ret;

    ret = bus_init(&pci_bus, &pci_bus_type);
    if (ret != STATUS_SUCCESS)
        return ret;

    ret = platform_pci_init();
    if (ret != STATUS_SUCCESS) {
        bus_destroy(&pci_bus);
        return ret;
    }

    return STATUS_SUCCESS;
}

static status_t pci_unload(void) {
    return STATUS_NOT_IMPLEMENTED;
}

MODULE_NAME(PCI_MODULE_NAME);
MODULE_DESC("PCI bus manager");
MODULE_FUNCS(pci_init, pci_unload);
