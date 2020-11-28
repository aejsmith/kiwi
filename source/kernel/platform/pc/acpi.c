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
 * @brief               PC ACPI functions.
 */

#include <pc/acpi.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>

/** Whether ACPI is supported. */
bool acpi_supported;

/** Array of pointers to copies of ACPI tables. */
static acpi_header_t **acpi_tables;
static size_t acpi_table_count;

static __init_text void acpi_table_copy(phys_ptr_t addr) {
    acpi_header_t *source = phys_map(addr, PAGE_SIZE * 2, MM_BOOT);

    /* Check the checksum of the table. */
    if (!checksum_range(source, source->length)) {
        phys_unmap(source, PAGE_SIZE * 2, true);
        return;
    }

    kprintf(
        LOG_NOTICE, "acpi: table %.4s revision %" PRIu8 " (%.6s %.8s %" PRIu32 ")\n",
        source->signature, source->revision, source->oem_id,
        source->oem_table_id, source->oem_revision);

    /* Reallocate the table array. */
    acpi_tables = krealloc(acpi_tables, sizeof(acpi_header_t *) * ++acpi_table_count, MM_BOOT);

    /* Allocate a table structure and copy the table. */
    acpi_tables[acpi_table_count - 1] = kmalloc(source->length, MM_BOOT);
    memcpy(acpi_tables[acpi_table_count - 1], source, source->length);

    phys_unmap(source, PAGE_SIZE * 2, true);
}

static inline acpi_rsdp_t *acpi_find_rsdp(phys_ptr_t start, size_t size) {
    assert(!(start % 16));
    assert(!(size % 16));

    /* Search through the range on 16-byte boundaries. */
    for (size_t i = 0; i < size; i += 16) {
        acpi_rsdp_t *rsdp = phys_map(start + i, sizeof(*rsdp), MM_BOOT);

        /* Check if the signature and checksum are correct. */
        if (strncmp((char *)rsdp->signature, ACPI_RSDP_SIGNATURE, 8) != 0) {
            phys_unmap(rsdp, sizeof(*rsdp), true);
            continue;
        } else if (!checksum_range(rsdp, 20)) {
            phys_unmap(rsdp, sizeof(*rsdp), true);
            continue;
        }

        /* If the revision is 2 or higher, then checksum the extended fields as
         * well. */
        if (rsdp->revision >= 2) {
            if (!checksum_range(rsdp, rsdp->length)) {
                phys_unmap(rsdp, sizeof(*rsdp), true);
                continue;
            }
        }

        kprintf(
            LOG_NOTICE, "acpi: found ACPI RSDP at 0x%" PRIxPHYS " (revision: %" PRIu8 ")\n",
            start + i, rsdp->revision);

        return rsdp;
    }

    return NULL;
}

static inline bool acpi_parse_xsdt(uint32_t addr) {
    acpi_xsdt_t *source = phys_map(addr, PAGE_SIZE, MM_BOOT);

    /* Check signature and checksum. */
    if (strncmp((char *)source->header.signature, ACPI_XSDT_SIGNATURE, 4) != 0) {
        kprintf(LOG_WARN, "acpi: XSDT signature does not match expected signature\n");
        phys_unmap(source, PAGE_SIZE, true);
        return false;
    } else if (!checksum_range(source, source->header.length)) {
        kprintf(LOG_WARN, "acpi: XSDT checksum is incorrect\n");
        phys_unmap(source, PAGE_SIZE, true);
        return false;
    }

    /* Load each table. */
    size_t count = (source->header.length - sizeof(source->header)) / sizeof(source->entry[0]);
    for (size_t i = 0; i < count; i++)
        acpi_table_copy((phys_ptr_t)source->entry[i]);

    phys_unmap(source, PAGE_SIZE, true);

    acpi_supported = true;
    return true;
}

static inline bool acpi_parse_rsdt(uint32_t addr) {
    acpi_rsdt_t *source = phys_map(addr, PAGE_SIZE, MM_BOOT);

    /* Check signature and checksum. */
    if (strncmp((char *)source->header.signature, ACPI_RSDT_SIGNATURE, 4) != 0) {
        kprintf(LOG_WARN, "acpi: RSDT signature does not match expected signature\n");
        phys_unmap(source, PAGE_SIZE, true);
        return false;
    } else if (!checksum_range(source, source->header.length)) {
        kprintf(LOG_WARN, "acpi: RSDT checksum is incorrect\n");
        phys_unmap(source, PAGE_SIZE, true);
        return false;
    }

    /* Load each table. */
    size_t count = (source->header.length - sizeof(source->header)) / sizeof(source->entry[0]);
    for (size_t i = 0; i < count; i++)
        acpi_table_copy((phys_ptr_t)source->entry[i]);

    phys_unmap(source, PAGE_SIZE, true);

    acpi_supported = true;
    return true;
}

/** Find an ACPI table.
 * @param signature     Signature of table to find.
 * @return              Pointer to table if found, NULL if not. */
acpi_header_t *acpi_table_find(const char *signature) {
    for (size_t i = 0; i < acpi_table_count; i++) {
        if (strncmp((char *)acpi_tables[i]->signature, signature, 4) != 0)
            continue;

        return acpi_tables[i];
    }

    return NULL;
}

/** Detect ACPI presence and find needed tables. */
__init_text void acpi_init(void) {
    /* Get the base address of the Extended BIOS Data Area (EBDA). */
    uint16_t *mapping = phys_map(0x40e, sizeof(uint16_t), MM_BOOT);
    phys_ptr_t ebda   = (*mapping) << 4;
    phys_unmap(mapping, sizeof(uint16_t), true);

    /* Search for the RSDP. */
    acpi_rsdp_t *rsdp = acpi_find_rsdp(ebda, 0x400);
    if (!rsdp) {
        rsdp = acpi_find_rsdp(0xe0000, 0x20000);
        if (!rsdp)
            return;
    }

    /* Create a copy of all the tables using the XSDT where possible. */
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        if (!acpi_parse_xsdt(rsdp->xsdt_address)) {
            if (rsdp->rsdt_address != 0)
                acpi_parse_rsdt(rsdp->rsdt_address);
        }
    } else if (rsdp->rsdt_address != 0) {
        acpi_parse_rsdt(rsdp->rsdt_address);
    } else {
        kprintf(LOG_WARN, "acpi: RSDP contains neither an XSDT or RSDT address, not using ACPI\n");
    }

    phys_unmap(rsdp, sizeof(*rsdp), true);
}
