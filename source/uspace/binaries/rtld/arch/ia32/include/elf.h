/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		IA32 RTLD ELF definitions
 */

#ifndef __RTLD_ARCH_ELF_H
#define __RTLD_ARCH_ELF_H

#include <elf.h>

/** Macro to get the correct ELF type. */
#define _ElfW(type)		Elf32_##type
#define ElfW(type)		_ElfW(type)

/** Relocation types. */
#define ELF_DT_REL_TYPE		ELF_DT_REL
#define ELF_DT_RELSZ_TYPE	ELF_DT_RELSZ
#define ELF_DT_RELENT_TYPE	ELF_DT_RELENT
#define ELF_REL_TYPE		Rel

/** Machine type definitions. */
#define ELF_CLASS		ELFCLASS32
#define ELF_ENDIAN		ELFDATA2LSB
#define ELF_MACHINE		ELF_EM_386

/* FIXME: Better place for this. */
#define PAGE_SIZE		0x1000

#endif /* __RTLD_ARCH_ELF_H */
