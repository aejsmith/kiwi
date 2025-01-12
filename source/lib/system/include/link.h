/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Dynamic linker interface.
 */

#pragma once

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>

__SYS_EXTERN_C_BEGIN

#define ElfW(type)          _ElfW(Elf, __WORDSIZE, type)
#define _ElfW(e,w,t)        _ElfW_1(e, w, _##t)
#define _ElfW_1(e,w,t)      e##w##t

/** Compatibility definitions. */
#define PT_LOAD             ELF_PT_LOAD
#define PT_GNU_EH_FRAME     ELF_PT_GNU_EH_FRAME

/** Structure containing information about a loaded shared object. */
struct dl_phdr_info {
    ElfW(Addr) dlpi_addr;           /**< Base address of object. */
    const char *dlpi_name;          /**< Name of object. */
    const ElfW(Phdr) *dlpi_phdr;    /**< Pointer to array of program headers. */
    ElfW(Half) dlpi_phnum;          /**< Number of program headers. */
};

extern int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info *, size_t, void *),
    void *data);

__SYS_EXTERN_C_END
