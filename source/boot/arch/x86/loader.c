/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		x86 kernel loader.
 *
 * Both AMD64 and IA32 create a 1GB identity mapping at the start of the
 * virtual address space. All paging structures are allocated as reclaimable,
 * as the kernel will create its own paging structures.
 */

#include <arch/boot.h>
#include <arch/features.h>
#include <arch/page.h>
#include <arch/sysreg.h>

#include <boot/cpu.h>
#include <boot/elf.h>
#include <boot/memory.h>

#include <lib/utility.h>

#include <assert.h>
#include <fatal.h>
#include <kargs.h>

extern void arch_enter_kernel64(kernel_args_t *args, uint32_t cpu, ptr_t cr3, uint64_t entry) __noreturn;
extern void arch_enter_kernel32(kernel_args_t *args, uint32_t cpu, ptr_t cr3, uint32_t entry) __noreturn;

/** Information on the loaded kernel. */
static bool kernel_is_64bit = false;
static Elf32_Addr kernel_entry32;
static Elf64_Addr kernel_entry64;
static ptr_t kernel_cr3;

/** IA32 kernel loader function. */
DEFINE_ELF_LOADER(load_elf32_kernel, 32, LARGE_PAGE_SIZE);

/** AMD64 kernel loader function. */
DEFINE_ELF_LOADER(load_elf64_kernel, 64, LARGE_PAGE_SIZE);

/** Allocate a paging structure.
 * @return		Pointer to structure. */
static uint64_t *allocate_paging_structure(void) {
	phys_ptr_t addr;
	uint64_t *ret;

	addr = phys_memory_alloc(PAGE_SIZE, PAGE_SIZE, true);
	ret = (uint64_t *)((ptr_t)addr);
	memset(ret, 0, PAGE_SIZE);
	return ret;
}

/** Load a 64-bit kernel image.
 * @param file		File containing the kernel.
 * @return		Whether the kernel is 64-bit. */
static bool arch_load_kernel64(vfs_node_t *file) {
	uint64_t *pml4, *pdp, *pdir;
	Elf64_Addr virt_base;
	size_t load_size, i;
	int pdpe, pde;

	if(!elf_check(file, ELFCLASS64, ELFDATA2LSB, ELF_EM_X86_64)) {
		return false;
	}

	/* Check for long mode support (booting_cpu is still set to the BSP
	 * at this point). */
	if(!CPU_HAS_LMODE(booting_cpu)) {
		fatal("64-bit kernel requires 64-bit CPU");
	}

	load_elf64_kernel(file, &kernel_entry64, &virt_base, &load_size);

	assert(virt_base >= 0xFFFFFFFF80000000LL);

	/* Identity map the first 1GB of physical memory. */
	pml4 = allocate_paging_structure();
	pdp = allocate_paging_structure();
	pdir = allocate_paging_structure();
	pml4[0] = (ptr_t)pdp | PG_PRESENT | PG_WRITE;
	pdp[0] = (ptr_t)pdir | PG_PRESENT | PG_WRITE;
	for(i = 0; i < 512; i++) {
		pdir[i] = (i * LARGE_PAGE_SIZE) | PG_PRESENT | PG_WRITE | PG_LARGE;
	}

	/* Map the kernel in. */
	pml4[511] = (ptr_t)pdp | PG_PRESENT | PG_WRITE;
	pdir = allocate_paging_structure();
	pdpe = (virt_base % 0x8000000000LL) / 0x40000000;
	pdp[pdpe] = (ptr_t)pdir | PG_PRESENT | PG_WRITE;
	pde = (virt_base % 0x40000000) / LARGE_PAGE_SIZE;
	for(i = 0; i < ROUND_UP(load_size, LARGE_PAGE_SIZE) / LARGE_PAGE_SIZE; i++) {
		pdir[pde + i] = (kernel_args->kernel_phys + (i * LARGE_PAGE_SIZE)) | PG_PRESENT | PG_WRITE | PG_LARGE;
	}

	/* Save details for later use. */
	kernel_is_64bit = true;
	kernel_cr3 = (ptr_t)pml4;
	dprintf("loader: 64-bit kernel entry point is 0x%llx, CR3 is %p\n",
	        kernel_entry64, kernel_cr3);
	return true;
}

/** Load a 32-bit kernel image.
 * @param file		File containing the kernel.
 * @return		Whether the kernel is 32-bit. */
static bool arch_load_kernel32(vfs_node_t *file) {
	Elf32_Addr virt_base;
	uint64_t *pdp, *pdir;
	size_t load_size, i;
	int pde;

	if(!elf_check(file, ELFCLASS32, ELFDATA2LSB, ELF_EM_386)) {
		return false;
	}

	load_elf32_kernel(file, &kernel_entry32, &virt_base, &load_size);

	assert(virt_base >= 0xC0000000);

	/* Identity map the first 1GB of physical memory. */
	pdp = allocate_paging_structure();
	pdir = allocate_paging_structure();
	pdp[0] = (ptr_t)pdir | PG_PRESENT;
	for(i = 0; i < 512; i++) {
		pdir[i] = (i * LARGE_PAGE_SIZE) | PG_PRESENT | PG_WRITE | PG_LARGE;
	}

	/* Map the kernel in. */
	pdir = allocate_paging_structure();
	pdp[3] = (ptr_t)pdir | PG_PRESENT;
	pde = (virt_base % 0x40000000) / LARGE_PAGE_SIZE;
	for(i = 0; i < ROUND_UP(load_size, LARGE_PAGE_SIZE) / LARGE_PAGE_SIZE; i++) {
		pdir[pde + i] = (kernel_args->kernel_phys + (i * LARGE_PAGE_SIZE)) | PG_PRESENT | PG_WRITE | PG_LARGE;
	}

	/* Save details for later use. */
	kernel_cr3 = (ptr_t)pdp;
	dprintf("loader: 32-bit kernel entry point is %p, CR3 is %p\n",
	        kernel_entry32, kernel_cr3);
	return true;
}

/** Load the kernel into memory.
 * @param file		File containing the kernel. */
void arch_load_kernel(vfs_node_t *file) {
	if(!arch_load_kernel64(file)) {
		if(!arch_load_kernel32(file)) {
			fatal("Kernel format is invalid");
		}
	}
}

/** Enter the kernel. */
void arch_enter_kernel(void) {
	/* All CPUs should reach this point simultaneously. Reset the TSC to
	 * 0, so that the kernel's timing functions return a consistent value
	 * on all CPUs. */
	sysreg_msr_write(SYSREG_MSR_TSC, 0);

	if(kernel_is_64bit) {
		arch_enter_kernel64(kernel_args, cpu_current_id(), kernel_cr3, kernel_entry64);
	} else {
		arch_enter_kernel32(kernel_args, cpu_current_id(), kernel_cr3, kernel_entry32);
	}
}
