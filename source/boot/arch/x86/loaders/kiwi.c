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
 * @brief		x86 Kiwi kernel loader.
 *
 * Both AMD64 and IA32 create a 1GB identity mapping at the start of the
 * virtual address space. All paging structures are allocated as reclaimable,
 * as the kernel will create its own paging structures.
 */

#include <arch/boot.h>
#include <arch/cpu.h>
#include <arch/page.h>

#include <boot/cpu.h>
#include <boot/elf.h>
#include <boot/error.h>
#include <boot/loader.h>
#include <boot/memory.h>

#include <lib/utility.h>

#include <assert.h>
#include <kargs.h>

/** Required CPU features (FPU, TSC, PAE, PGE, FXSR). */
#define REQUIRED_FEATURES	((1<<0) | (1<<4) | (1<<6) | (1<<13) | (1<<24))

extern void kiwi_loader_arch_enter64(kernel_args_t *args, uint32_t cpu, ptr_t cr3, uint64_t entry) __noreturn;
extern void kiwi_loader_arch_enter32(kernel_args_t *args, uint32_t cpu, ptr_t cr3, uint32_t entry) __noreturn;

/** Information on the loaded kernel. */
static bool kernel_is_64bit = false;
static Elf32_Addr kernel_entry32;
static Elf64_Addr kernel_entry64;
static ptr_t kernel_cr3;

/** IA32 kernel loader function. */
DEFINE_ELF_LOADER(load_elf32_kernel, 32, LARGE_PAGE_SIZE);

/** AMD64 kernel loader function. */
DEFINE_ELF_LOADER(load_elf64_kernel, 64, LARGE_PAGE_SIZE);

/** Set up x86-specific Kiwi options in an environment.
 * @param env		Environment to set up. */
void kiwi_loader_arch_setup(environ_t *env) {
	value_t value;

	if(!environ_lookup(env, "lapic_disabled")) {
		value.type = VALUE_TYPE_BOOLEAN;
		value.boolean = false;
		environ_insert(env, "lapic_disabled", &value);
	}
}

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
 * @param handle	Handle to file containing kernel.
 * @return		Whether the kernel is 64-bit. */
static bool kiwi_loader_arch_load64(fs_handle_t *handle) {
	uint64_t *pml4, *pdp, *pdir;
	Elf64_Addr virt_base;
	size_t load_size, i;
	int pdpe, pde;

	if(!elf_check(handle, ELFCLASS64, ELFDATA2LSB, ELF_EM_X86_64)) {
		return false;
	}

	/* Check whether long mode is supported. Here I would check for SYSCALL
	 * support, too, but Intel are twats and don't set the SYSCALL bit in
	 * the CPUID information unless you're in 64-bit mode. */
	if(!(kernel_args->arch.extended_edx & (1<<29))) {
		boot_error("64-bit kernel requires 64-bit CPU");
	}

	load_elf64_kernel(handle, &kernel_entry64, &virt_base, &load_size);

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
 * @param handle	Handle to file containing kernel.
 * @return		Whether the kernel is 32-bit. */
static bool kiwi_loader_arch_load32(fs_handle_t *handle) {
	Elf32_Addr virt_base;
	uint64_t *pdp, *pdir;
	size_t load_size, i;
	int pde;

	if(!elf_check(handle, ELFCLASS32, ELFDATA2LSB, ELF_EM_386)) {
		return false;
	}

	load_elf32_kernel(handle, &kernel_entry32, &virt_base, &load_size);

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

/** Load a Kiwi kernel.
 * @param handle	Handle to the file to load.
 * @param env		Environment for the kernel. */
void kiwi_loader_arch_load(fs_handle_t *handle, environ_t *env) {
	value_t *value;

	/* Pull settings out of the environment into the kernel arguments. */
	value = environ_lookup(env, "lapic_disabled");
	kernel_args->arch.lapic_disabled = value->boolean;

	/* Check if the LAPIC is available. */
	if(!kernel_args->arch.lapic_disabled && cpu_lapic_init()) {
		/* Set the real ID of the boot CPU. */
		boot_cpu->id = cpu_current_id();
		if(boot_cpu->id > kernel_args->highest_cpu_id) {
			kernel_args->highest_cpu_id = boot_cpu->id;
		}
	} else {
		/* Force SMP to be disabled if the boot CPU does not have a
		 * local APIC or if it has been manually disabled. */
		kernel_args->arch.lapic_disabled = true;
		kernel_args->smp_disabled = true;
	}

	/* Check that features required on both 32- and 64-bit kernels are
	 * supported. */
	if((kernel_args->arch.standard_edx & REQUIRED_FEATURES) != REQUIRED_FEATURES) {
		boot_error("Required CPU features not present on all CPUs");
	}

	if(!kiwi_loader_arch_load64(handle)) {
		if(!kiwi_loader_arch_load32(handle)) {
			boot_error("Kernel image format is invalid");
		}
	}
}

/** Add x86-specific Kiwi options to a configuration window.
 * @param env		Environment for the kernel.
 * @param window	List window to add to. */
void kiwi_loader_arch_configure(environ_t *env, ui_window_t *window) {
	ui_list_insert_env(window, env, "lapic_disabled", "Disable Local APIC usage", false);
}

/** Enter the loaded kernel. */
void __noreturn kiwi_loader_arch_enter(void) {
	/* All CPUs should reach this point simultaneously. Reset the TSC to
	 * 0, so that the kernel's timing functions return a consistent value
	 * on all CPUs. */
	x86_write_msr(X86_MSR_TSC, 0);

#if CONFIG_X86_NX
	/* Enable NX/XD if supported (only bother if it is supported on all
	 * CPUs, as the kernel won't use it if it isn't). */
	if(kernel_args->arch.extended_edx & (1<<20)) {
		x86_write_msr(X86_MSR_EFER, x86_read_msr(X86_MSR_EFER) | X86_EFER_NXE);
	}
#endif

	if(kernel_is_64bit) {
		kiwi_loader_arch_enter64(kernel_args, cpu_current_id(), kernel_cr3, kernel_entry64);
	} else {
		kiwi_loader_arch_enter32(kernel_args, cpu_current_id(), kernel_cr3, kernel_entry32);
	}
}
