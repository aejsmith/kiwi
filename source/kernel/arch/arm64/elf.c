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
 * @brief               ARM64 ELF helper functions.
 */

#include <elf.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

/** Perform a REL relocation on an ELF module. */
status_t arch_elf_module_relocate_rel(elf_image_t *image, elf_rel_t *rel, elf_shdr_t *target) {
    kprintf(LOG_WARN, "elf: REL relocation section unsupported\n");
    return STATUS_NOT_IMPLEMENTED;
}

/** Perform a RELA relocation on an ELF module. */
status_t arch_elf_module_relocate_rela(elf_image_t *image, elf_rela_t *rel, elf_shdr_t *target) {
    kprintf(LOG_DEBUG, "TODO\n");
    return STATUS_NOT_IMPLEMENTED;
}
