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
 * @brief               AMD64 MMU definitions.
 */

#pragma once

#include <types.h>

/** Definitions of paging structure bits. */
#define X86_PTE_PRESENT                 (1ul<<0)    /**< Page is present. */
#define X86_PTE_WRITE                   (1ul<<1)    /**< Page is writable. */
#define X86_PTE_USER                    (1ul<<2)    /**< Page is accessible in CPL3. */
#define X86_PTE_PWT                     (1ul<<3)    /**< Page has write-through caching. */
#define X86_PTE_PCD                     (1ul<<4)    /**< Page has caching disabled. */
#define X86_PTE_ACCESSED                (1ul<<5)    /**< Page has been accessed. */
#define X86_PTE_DIRTY                   (1ul<<6)    /**< Page has been written to. */
#define X86_PTE_LARGE                   (1ul<<7)    /**< Page is a large page. */
#define X86_PTE_GLOBAL                  (1ul<<8)    /**< Page won't be cleared in TLB. */
#define X86_PTE_NOEXEC                  (1ul<<63)   /**< Page is not executable (requires NX support). */

/** Protection flag mask. */
#define X86_PTE_PROTECT_MASK            (X86_PTE_WRITE | X86_PTE_NOEXEC)

/** Cacheability flag mask. */
#define X86_PTE_CACHE_MASK              (X86_PTE_PWT | X86_PTE_PCD)

/** PAT entry values. */
#define X86_PAT_UC                      0x00
#define X86_PAT_WC                      0x01
#define X86_PAT_WT                      0x04
#define X86_PAT_WP                      0x05
#define X86_PAT_WB                      0x06
#define X86_PAT_UC_MINUS                0x07

/** PAT indices corresponding to MMU_CACHE_* types. */
#define X86_PAT_INDEX_NORMAL            0
#define X86_PAT_INDEX_WRITE_COMBINE     2
#define X86_PAT_INDEX_UNCACHED          3

/**
 * PAT selectors. Note that currently we only use the PCD and PWT bits - if we
 * add more types such that we need to use the PAT bit, this needs to be handled
 * separately for large and small page mappings, since the PAT bit is in
 * different positions for these.
 */
#define X86_PTE_PAT_SELECT(idx)         (((uint64_t)(idx) & 3) << 3)
#define X86_PTE_PAT_NORMAL              X86_PTE_PAT_SELECT(X86_PAT_INDEX_NORMAL)
#define X86_PTE_PAT_WRITE_COMBINE       X86_PTE_PAT_SELECT(X86_PAT_INDEX_WRITE_COMBINE)
#define X86_PTE_PAT_UNCACHED            X86_PTE_PAT_SELECT(X86_PAT_INDEX_UNCACHED)

/**
 * PAT value matching the above selectors. Unused fields are set matching their
 * default reset value according to the Intel manual.
 */
#define X86_PAT_ENTRY(idx, val)         ((uint64_t)(val) << ((idx) * 8))
#define X86_PAT ( \
    X86_PAT_ENTRY(0, X86_PAT_WB) | \
    X86_PAT_ENTRY(1, X86_PAT_WT) | \
    X86_PAT_ENTRY(2, X86_PAT_WC) | \
    X86_PAT_ENTRY(3, X86_PAT_UC) | \
    X86_PAT_ENTRY(4, X86_PAT_WB) | \
    X86_PAT_ENTRY(5, X86_PAT_WT) | \
    X86_PAT_ENTRY(6, X86_PAT_UC_MINUS) | \
    X86_PAT_ENTRY(7, X86_PAT_UC))
