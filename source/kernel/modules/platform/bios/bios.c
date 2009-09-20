/* Kiwi PC BIOS interrupt interface
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
 * @brief		PC BIOS interrupt interface.
 */

#include <arch/io.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <mm/malloc.h>
#include <mm/vmem.h>

#include <platform/bios.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>
#include <module.h>

#include "x86emu/x86emu.h"

/** BIOS memory area definitions. */
#define BIOS_BDA_BASE		0
#define BIOS_BDA_SIZE		0x1000
#define BIOS_EBDA_BASE		0x90000
#define BIOS_EBDA_SIZE		0x70000
#define BIOS_MEM_BASE		0x1000
#define BIOS_MEM_SIZE		0x8F000
#define BIOS_STACK_SIZE		0x1000

/** BIOS interrupt request structure. */
typedef struct bios_request {
	list_t header;			/**< Link to request queue. */
	uint8_t num;			/**< Interrupt number. */
	bios_regs_t *regs;		/**< Registers structure. */
	semaphore_t sem;		/**< Semaphore to wait for completion on. */
} bios_request_t;

/** BIOS memory allocation data. */
static void *bios_mem_area = NULL;
static size_t bios_allocation_count = 0;
static vmem_t *bios_mem_arena = NULL;
static void *bios_bda = NULL;
static void *bios_ebda = NULL;

/** BIOS interrupt request queue. */
static LIST_DECLARE(bios_queue);
static SEMAPHORE_DECLARE(bios_sem, 0);
static thread_t *bios_thread = NULL;

/** Lock to protect request queue/allocation data. */
static MUTEX_DECLARE(bios_lock, 0);

#if 0
# pragma mark X86EMU helper functions.
#endif

/** Read 8 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint8_t x86emu_pio_inb(uint16_t port) {
	return in8(port);
}

/** Write 8 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outb(uint16_t port, uint8_t data) {
	out8(port, data);
}

/** Read 16 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint16_t x86emu_pio_inw(uint16_t port) {
	return in16(port);
}

/** Write 16 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outw(uint16_t port, uint16_t data) {
	out16(port, data);
}

/** Read 32 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static uint32_t x86emu_pio_inl(uint16_t port) {
	return in32(port);
}

/** Write 32 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static void x86emu_pio_outl(uint16_t port, uint32_t data) {
	out32(port, data);
}

/** X86EMU PIO helpers. */
static X86EMU_pioFuncs x86emu_pio_funcs = {
	.inb  = x86emu_pio_inb,
	.outb = x86emu_pio_outb,
	.inw  = x86emu_pio_inw,
	.outw = x86emu_pio_outw,
	.inl  = x86emu_pio_inl,
	.outl = x86emu_pio_outl,
};

/** Map an X86EMU address to the correct address.
 * @todo		Handle accesses that straddle across 2 memory areas.
 * @param phys		Address to convert.
 * @param addrp		Where to store converted address.
 * @return		Whether the address was valid. */
static bool x86emu_mem_convert(uint32_t phys, void **addrp) {
	if(phys < (BIOS_BDA_BASE + BIOS_BDA_SIZE)) {
		*addrp = bios_bda + (phys - BIOS_BDA_BASE);
		return true;
	} else if(phys >= BIOS_EBDA_BASE && phys < (BIOS_EBDA_BASE + BIOS_EBDA_SIZE)) {
		*addrp = bios_ebda + (phys - BIOS_EBDA_BASE);
		return true;
	} else if(phys >= BIOS_MEM_BASE && phys < (BIOS_MEM_BASE + BIOS_MEM_SIZE)) {
		*addrp = bios_mem_area + (phys - BIOS_MEM_BASE);
		return true;
	} else {
		return false;
	}
}

/** Read an 8-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint8_t x86emu_mem_rdb(uint32_t addr) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return 0;
	}

	return *(uint8_t *)dest;
}

/** Write an 8-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrb(uint32_t addr, uint8_t val) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return;
	}

	*(uint8_t *)dest = val;
}

/** Read a 16-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint16_t x86emu_mem_rdw(uint32_t addr) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return 0;
	}

	return *(uint16_t *)dest;
}

/** Write a 16-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrw(uint32_t addr, uint16_t val) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return;
	}

	*(uint16_t *)dest = val;
}

/** Read a 32-bit value.
 * @param addr		Address to read from.
 * @return		Value read. */
static uint32_t x86emu_mem_rdl(uint32_t addr) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return 0;
	}

	return *(uint32_t *)dest;
}

/** Write a 32-bit value.
 * @param addr		Address to write to.
 * @param val		Value to write. */
static void x86emu_mem_wrl(uint32_t addr, uint32_t val) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return;
	}

	*(uint32_t *)dest = val;
}

/** X86EMU memory helpers. */
static X86EMU_memFuncs x86emu_mem_funcs = {
	.rdb = x86emu_mem_rdb,
	.wrb = x86emu_mem_wrb,
	.rdw = x86emu_mem_rdw,
	.wrw = x86emu_mem_wrw,
	.rdl = x86emu_mem_rdl,
	.wrl = x86emu_mem_wrl,
};

#if 0
# pragma mark Main functions.
#endif

/** BIOS request thread. */
static void bios_thread_entry(void *arg1, void *arg2) {
	bios_request_t *request;
	void *halt, *stack;

	while(true) {
		semaphore_down(&bios_sem, 0);
		request = list_entry(bios_queue.next, bios_request_t, header);
		list_remove(&request->header);

		/* Allocate a stack and a halt byte to finish execution. */
		stack = bios_mem_alloc(BIOS_STACK_SIZE, MM_SLEEP);
		halt = bios_mem_alloc(1, MM_SLEEP);
		*(uint8_t *)halt = 0xF4;

		/* Copy in the registers. */
		memset(&M, 0, sizeof(M));
		M.x86.R_EAX  = request->regs->eax;
		M.x86.R_EBX  = request->regs->ebx;
		M.x86.R_ECX  = request->regs->ecx;
		M.x86.R_EDX  = request->regs->edx;
		M.x86.R_EDI  = request->regs->edi;
		M.x86.R_ESI  = request->regs->esi;
		M.x86.R_EBP  = request->regs->ebp;
		M.x86.R_ESP  = bios_mem_virt2phys(stack + BIOS_STACK_SIZE);
		M.x86.R_EFLG = SYSREG_FLAGS_IF | SYSREG_FLAGS_ALWAYS1;
		M.x86.R_EIP  = bios_mem_virt2phys(halt);
		M.x86.R_CS   = 0x0;
		M.x86.R_DS   = request->regs->ds;
		M.x86.R_ES   = request->regs->es;
		M.x86.R_FS   = request->regs->fs;
		M.x86.R_GS   = request->regs->gs;
		M.x86.R_SS   = 0x0;

		/* Run the interrupt. */
		X86EMU_prepareForInt(request->num);
		X86EMU_exec();

		/* Copy back modified registers. */
		request->regs->eax = M.x86.R_EAX;
		request->regs->ebx = M.x86.R_EBX;
		request->regs->ecx = M.x86.R_ECX;
		request->regs->edx = M.x86.R_EDX;
		request->regs->edi = M.x86.R_EDI;
		request->regs->esi = M.x86.R_ESI;
		request->regs->ebp = M.x86.R_EBP;
		request->regs->ds  = M.x86.R_DS;
		request->regs->es  = M.x86.R_ES;
		request->regs->fs  = M.x86.R_FS;
		request->regs->gs  = M.x86.R_GS;

		/* Free up data. */
		bios_mem_free(halt, 1);
		bios_mem_free(stack, BIOS_STACK_SIZE);

		/* Wake the caller. */
		semaphore_up(&request->sem, 1);
	}
}

/** Allocate memory to use for BIOS interrupts.
 *
 * Allocates a chunk of memory to use to pass data to BIOS interrupts.
 *
 * @param size		Size of allocation.
 * @param mmflag	Allocation flags.
 *
 * @return		Pointer to kernel address of allocation on success,
 *			NULL on failure.
 */
void *bios_mem_alloc(size_t size, int mmflag) {
	vmem_resource_t ret;

	mutex_lock(&bios_lock, 0);

	if(!(ret = vmem_alloc(bios_mem_arena, size, mmflag))) {
		mutex_unlock(&bios_lock);
		return NULL;
	} else if(bios_allocation_count++ == 0) {
		/* Previously had no allocations. Create the memory area. */
		if(!(bios_mem_area = kmalloc(BIOS_MEM_SIZE, mmflag))) {
			vmem_free(bios_mem_arena, ret, size);
			bios_allocation_count--;
			mutex_unlock(&bios_lock);
			return NULL;
		}
	}

	mutex_unlock(&bios_lock);
	return (void *)(((ptr_t)ret - BIOS_MEM_BASE) + (ptr_t)bios_mem_area);
}
MODULE_EXPORT(bios_mem_alloc);

/** Free BIOS memory.
 *
 * Frees a memory chunk previously allocated with bios_mem_alloc().
 *
 * @param addr		Address of allocation.
 * @param size		Size of the original allocation.
 */
void bios_mem_free(void *addr, size_t size) {
	vmem_resource_t base;

	mutex_lock(&bios_lock, 0);

	/* Get allocation address/size. */
	base = (vmem_resource_t)(((ptr_t)addr - (ptr_t)bios_mem_area) + BIOS_MEM_BASE);

	vmem_free(bios_mem_arena, base, size);
	if(--bios_allocation_count == 0) {
		kfree(bios_mem_area);
	}

	mutex_unlock(&bios_lock);
}
MODULE_EXPORT(bios_mem_free);

/** Convert a virtual address to a BIOS memory address.
 *
 * Converts a virtual address (i.e. from bios_mem_alloc()) to a real-mode
 * physical address. Can only convert addresses within the BIOS anonymous
 * memory space.
 *
 * @param addr		Virtual address.
 *
 * @return		Address that can be used by BIOS code.
 */
uint32_t bios_mem_virt2phys(void *addr) {
	return (uint32_t)(((ptr_t)addr - (ptr_t)bios_mem_area) + BIOS_MEM_BASE);
}
MODULE_EXPORT(bios_mem_virt2phys);

/** Convert a BIOS memory address to a virtual address.
 *
 * Converts a physical BIOS memory address to a virtual kernel address. Be
 * aware of data that may straddle across a boundary between different memory
 * areas.
 *
 * @param addr		Physical address.
 *
 * @return		Virtual address, or NULL if address was invalid.
 */
void *bios_mem_phys2virt(uint32_t addr) {
	void *dest;

	if(!x86emu_mem_convert(addr, &dest)) {
		return NULL;
	}

	return dest;
}
MODULE_EXPORT(bios_mem_phys2virt);

/** Execute a BIOS interrupt.
 *
 * Executes a real-mode BIOS interrupt. Accesses to BIOS interrupts are
 * serialized.
 *
 * @param num		Interrupt number to call.
 * @param regs		Registers to pass to interrupt. Will be updated with
 *			the register state after calling the interrupt.
 *
 * @return		0 on success, negative error code on failure.
 */
int bios_interrupt(uint8_t num, bios_regs_t *regs) {
	bios_request_t request;

	if(!regs) {
		return -ERR_PARAM_INVAL;
	}

	list_init(&request.header);
	semaphore_init(&request.sem, "bios_request_sem", 0);
	request.num = num;
	request.regs = regs;

	mutex_lock(&bios_lock, 0);
	list_append(&bios_queue, &request.header);
	semaphore_up(&bios_sem, 1);
	mutex_unlock(&bios_lock);

	semaphore_down(&request.sem, 0);
	return 0;
}
MODULE_EXPORT(bios_interrupt);

/** Initialisation function for the BIOS module.
 * @return		0 on success, negative error code on failure. */
static int bios_init(void) {
	int ret;

	/* Create the BIOS interrupt thread. */
	if((ret = thread_create("bios", kernel_proc, 0, bios_thread_entry, NULL, NULL, &bios_thread)) != 0) {
		return ret;
	}

	/* Initialise BIOS memory allocator. */
	bios_mem_arena = vmem_create("bios_mem_arena", BIOS_MEM_BASE, BIOS_MEM_SIZE,
	                             1, NULL, NULL, NULL, 0, 0, MM_SLEEP);

	/* Initialise the I/O and memory functions for X86EMU. */
	X86EMU_setupPioFuncs(&x86emu_pio_funcs);
	X86EMU_setupMemFuncs(&x86emu_mem_funcs);

	/* Map in the BDA/EBDA. */
	bios_bda = page_phys_map(BIOS_BDA_BASE, BIOS_BDA_SIZE, MM_SLEEP);
	bios_ebda = page_phys_map(BIOS_EBDA_BASE, BIOS_EBDA_SIZE, MM_SLEEP);

	thread_wire(bios_thread);
	thread_run(bios_thread);
	return 0;
}

/** Unloading function for the BIOS module.
 * @return		0 on success, negative error code on failure. */
static int bios_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("bios");
MODULE_DESC("PC BIOS interrupt interface");
MODULE_FUNCS(bios_init, bios_unload);
