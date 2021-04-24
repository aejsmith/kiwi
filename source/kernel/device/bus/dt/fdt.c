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
 * @brief               FDT (low-level) device tree interface.
 */

#include <device/bus/fdt.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <kboot.h>
#include <kernel.h>

static void *fdt_address;
static uint32_t fdt_size;

/** Get the FDT address. */
const void *fdt_get(void) {
    return fdt_address;
}

/** Initialize the FDT. */
__init_text void fdt_init(void) {
    kboot_tag_fdt_t *tag = kboot_tag_iterate(KBOOT_TAG_FDT, NULL);
    if (!tag)
        fatal("Boot loader did not supply FDT");

    /* Make our own copy of the FDT since KBoot puts it in reclaimable memory. */
    fdt_size    = tag->size;
    fdt_address = kmalloc(fdt_size, MM_BOOT);
    memcpy(fdt_address, (void *)((ptr_t)tag->addr_virt), fdt_size);

    int ret = fdt_check_header(fdt_address);
    if (ret != 0)
        fatal("FDT header validation failed (%d)", ret);
}
