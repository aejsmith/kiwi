/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		x86 KBoot kernel loader.
 *
 * @todo		Only use large pages if supported.
 */

#include <arch/mmu.h>

#include <x86/cpu.h>

#include <elf.h>
#include <fs.h>
#include <kboot.h>
#include <loader.h>

extern mmu_context_t *kboot_arch_load(fs_handle_t *handle);

/** IA32 kernel loader function. */
DEFINE_ELF_LOADER(load_elf32_kernel, 32, 0x400000);

/** AMD64 kernel loader function. */
DEFINE_ELF_LOADER(load_elf64_kernel, 64, 0x200000);

/** Information on the loaded kernel. */
static bool kernel_is_64bit = false;
static Elf32_Addr kernel_entry32;
static Elf64_Addr kernel_entry64;

/** Check for long mode support.
 * @return		Whether long mode is supported. */
static inline bool have_long_mode(void) {
	uint32_t eax, ebx, ecx, edx;

	/* Check whether long mode is supported. */
	x86_cpuid(X86_CPUID_EXT_MAX, &eax, &ebx, &ecx, &edx);
	if(eax & (1<<31)) {
		x86_cpuid(X86_CPUID_EXT_FEATURE, &eax, &ebx, &ecx, &edx);
		if(edx & (1<<29)) {
			return true;
		}
	}

	return false;
}

/** Load an AMD64 KBoot image into memory.
 * @param handle	Handle to image.
 * @return		Created MMU context for kernel. */
static mmu_context_t *kboot_arch_load64(fs_handle_t *handle) {
	mmu_context_t *ctx;

	/* Check for 64-bit support. */
	if(!have_long_mode()) {
		boot_error("64-bit kernel requires 64-bit CPU");
	}

	/* Create the MMU context. */
	ctx = mmu_create(true);

	/* Load the kernel. */
	load_elf64_kernel(handle, ctx, &kernel_entry64);
	kernel_is_64bit = true;
	dprintf("kboot: 64-bit kernel entry point is 0x%llx, CR3 is 0x%llx\n",
	        kernel_entry64, ctx->cr3);
	return ctx;
}

/** Load an IA32 KBoot image into memory.
 * @param handle	Handle to image.
 * @return		Created MMU context for kernel. */
static mmu_context_t *kboot_arch_load32(fs_handle_t *handle) {
	mmu_context_t *ctx;

	/* Create the MMU context. */
	ctx = mmu_create(false);

	/* Load the kernel. */
	load_elf32_kernel(handle, ctx, &kernel_entry32);
	dprintf("kboot: 32-bit kernel entry point is 0x%lx, CR3 is 0x%llx\n",
	        kernel_entry32, ctx->cr3);
	return ctx;
}

/** Load a KBoot image into memory.
 * @param handle	Handle to image.
 * @return		Created MMU context for kernel. */
mmu_context_t *kboot_arch_load(fs_handle_t *handle) {
	mmu_context_t *ctx;

	if(elf_check(handle, ELFCLASS64, ELF_EM_X86_64)) {
		ctx = kboot_arch_load64(handle);
	} else if(elf_check(handle, ELFCLASS32, ELF_EM_386)) {
		ctx = kboot_arch_load32(handle);
	} else {
		boot_error("Kernel image is not for this architecture");
	}

	/* Identity map the loader (first 4MB). */
	mmu_map(ctx, 0, 0, 0x400000);
	return ctx;
}
