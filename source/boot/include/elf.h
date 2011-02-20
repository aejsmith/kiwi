/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		ELF loading functions.
 */

#ifndef __BOOT_ELF_H
#define __BOOT_ELF_H

#include "../../include/elf.h"

#include <fs.h>

/** ELF note type. */
typedef Elf32_Note elf_note_t;

/** ELF note iteration function.
 * @param note		Note header.
 * @param name		Note name.
 * @param desc		Note data.
 * @param data		Data pointer passed to elf_note_iterate().
 * @return		Whether to continue iteration. */
typedef bool (*elf_note_iterate_t)(elf_note_t *note, const char *name, void *desc, void *data);

extern bool elf_note_iterate(fs_handle_t *handle, elf_note_iterate_t cb, void *data);

#endif /* __BOOT_ELF_H */
