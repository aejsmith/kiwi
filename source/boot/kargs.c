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
 * @brief		Kernel arguments structure functions.
 */

#include <boot/memory.h>
#include <lib/string.h>
#include <kargs.h>

/** Arguments structure passed to the kernel. */
kernel_args_t *g_kernel_args = NULL;

/** Pointer to the currently booting CPU. */
kernel_args_cpu_t *g_booting_cpu = NULL;

/** Pointer to the boot CPU. */
kernel_args_cpu_t *g_boot_cpu = NULL;

/** Add a CPU to the kernel arguments structure.
 * @param id		ID of the CPU. */
kernel_args_cpu_t *kargs_cpu_add(uint32_t id) {
	kernel_args_cpu_t *cpu, *exist;
	phys_ptr_t addr;

	cpu = kmalloc(sizeof(kernel_args_cpu_t));
	cpu->next = 0;
	cpu->id = id;

	if(!g_kernel_args->cpus) {
		g_kernel_args->cpus = (ptr_t)cpu;
		g_boot_cpu = cpu;
	} else {
		for(addr = g_kernel_args->cpus; addr; addr = exist->next) {
			exist = (kernel_args_cpu_t *)((ptr_t)addr);
		}

		exist->next = (ptr_t)cpu;
	}

	g_kernel_args->cpu_count++;
	if(id > g_kernel_args->highest_cpu_id) {
		g_kernel_args->highest_cpu_id = id;
	}

	return cpu;
}

/** Add a module to the kernel arguments.
 * @param base		Address of module.
 * @param size		Size of module. */
kernel_args_module_t *kargs_module_add(phys_ptr_t base, uint32_t size) {
	kernel_args_module_t *mod, *exist;
	phys_ptr_t addr;

	mod = kmalloc(sizeof(kernel_args_module_t));
	mod->next = 0;
	mod->base = base;
	mod->size = size;

	if(!g_kernel_args->modules) {
		g_kernel_args->modules = (ptr_t)mod;
	} else {
		for(addr = g_kernel_args->modules; addr; addr = exist->next) {
			exist = (kernel_args_module_t *)((ptr_t)addr);
		}

		exist->next = (ptr_t)mod;
	}

	g_kernel_args->module_count++;
	return mod;
}

/** Initialise the kernel arguments structure. */
void kargs_init(void) {
	g_kernel_args = kmalloc(sizeof(kernel_args_t));
	memset(g_kernel_args, 0, sizeof(kernel_args_t));
#if CONFIG_DEBUG
	/* Disable the boot splash by default for debug builds. */
	g_kernel_args->splash_disabled = true;
#endif
}