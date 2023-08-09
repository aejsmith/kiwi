/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               AMD64 interrupt handling functions.
 */

#include <x86/cpu.h>
#include <x86/fpu.h>
#include <x86/interrupt.h>

#include <device/irq.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/safe.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <kdb.h>
#include <kernel.h>
#include <setjmp.h>
#include <status.h>

extern void kdb_db_exception(frame_t *frame);
extern void interrrupt_handler(frame_t *frame);

/** Array of interrupt handling routines. */
interrupt_handler_t interrupt_table[IDT_ENTRY_COUNT];

/** String names for CPU exceptions. */
static const char *except_strings[] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Error",
};

/** Unhandled exception handler.
 * @param frame         Interrupt stack frame.
 * @param code          User exception code. If 0, will be treated as fatal. */
static void unhandled_exception(frame_t *frame, unsigned code) {
    exception_info_t exception;
    const char *string = (frame->num < array_size(except_strings))
        ? except_strings[frame->num]
        : "Reserved";

    if (atomic_load(&kdb_running) == 2) {
        kdb_exception(string, frame);
    } else if (code && frame_from_user(frame)) {
        kprintf(
            LOG_DEBUG, "arch: thread %" PRId32 " (%s) exception %lu (%s) at 0x%zx\n",
            curr_thread->id, curr_thread->name, frame->num, string, frame->ip);

        memset(&exception, 0, sizeof(exception));
        exception.code = code;
        thread_exception(&exception);
    } else {
        fatal_etc(
            frame, "Unhandled %s mode exception %lu (%s)",
            (frame_from_user(frame)) ? "user" : "kernel", frame->num, string);
    }
}

/** Handler for exceptions without a specific handler.
 * @param frame         Interrupt stack frame. */
static void unknown_exception(frame_t *frame) {
    /* We treat all exceptions that we do not have a separate handler for as
     * fatal, regardless of which mode we are in. We should not ever get #OF and
     * #BR exceptions as the INTO and BOUND instructions are not present in
     * 64-bit mode. #TS indicates that something is broken in the TSS, which is
     * fatal. Double fault and machine check are also fatal. TODO: Proper
     * machine check handler. */
    unhandled_exception(frame, 0);
}

/** Hardware interrupt wrapper.
 * @param frame         Interrupt stack frame. */
static void hardware_interrupt(frame_t *frame) {
    enter_interrupt();

    /* Hardware IRQs start at 32. */
    irq_handler(root_irq_domain, frame->num - 32);

    leave_interrupt();
}

/** Unhandled interrupt function.
 * @param frame         Interrupt stack frame. */
static void unknown_interrupt(frame_t *frame) {
    if (atomic_load(&kdb_running) == 2) {
        kdb_exception("Unknown", frame);
    } else {
        fatal_etc(frame, "Received unknown interrupt %lu", frame->num);
    }
}

/** Divide Error (#DE) exception handler.
 * @param frame         Interrupt stack frame. */
static void de_exception(frame_t *frame) {
    unhandled_exception(frame, EXCEPTION_INT_DIV_ZERO);
}

/** Handler for NMIs.
 * @param frame         Interrupt stack frame. */
static void nmi_interrupt(frame_t *frame) {
    if (atomic_load(&kdb_running) > 0 || atomic_load(&in_fatal) > 0) {
        while (atomic_load(&kdb_running) > 0 || atomic_load(&in_fatal) > 0)
            arch_cpu_spin_hint();
    } else {
        fatal_etc(frame, "Received unexpected NMI");
    }
}

/** Breakpoint (#BP) exception handler.
 * @param frame         Interrupt stack frame. */
static void bp_exception(frame_t *frame) {
    // TODO: User debugging.
    unhandled_exception(frame, EXCEPTION_BREAKPOINT);
}

/** Invalid Opcode (#UD) exception handler.
 * @param frame         Interrupt stack frame. */
static void ud_exception(frame_t *frame) {
    unhandled_exception(frame, EXCEPTION_INVALID_INSTRUCTION);
}

/** Handler for device-not-available (#NM) exceptions.
 * @param frame         Interrupt stack frame. */
static void nm_exception(frame_t *frame) {
    if (frame_from_user(frame)) {
        /* We're coming from user-mode, this is a valid request for FPU usage.
         * Enable the FPU. */
        x86_fpu_enable();

        /* If the thread has the ARCH_THREAD_HAVE_FPU flag set, we have used the
         * FPU previously and so have a state to restore. Otherwise, initialize
         * a new state. */
        if (curr_thread->arch.flags & ARCH_THREAD_HAVE_FPU) {
            x86_fpu_restore(curr_thread->arch.fpu);
        } else {
            x86_fpu_init();
            curr_thread->arch.flags |= ARCH_THREAD_HAVE_FPU;
        }

        if (++curr_thread->arch.fpu_count >= 5) {
            /* We're using the FPU frequently, set a flag which causes the FPU
             * state to be loaded during a thread switch. */
            curr_thread->arch.flags |= ARCH_THREAD_FREQUENT_FPU;
        }
    } else {
        /* FPU usage is not allowed in kernel-mode. */
        unhandled_exception(frame, 0);
    }
}

/** Segment Not Present (#NP) exception handler.
 * @param frame         Interrupt stack frame. */
static void np_exception(frame_t *frame) {
    unhandled_exception(frame, EXCEPTION_INVALID_INSTRUCTION);
}

/** Stack Fault (#SS) exception handler.
 * @param frame         Interrupt stack frame. */
static void ss_exception(frame_t *frame) {
    unhandled_exception(frame, EXCEPTION_INVALID_INSTRUCTION);
}

/** General Protection (#GP) exception handler.
 * @param frame         Interrupt stack frame. */
static void gp_exception(frame_t *frame) {
    /* Hmm. There is a wide range of conditions that can cause a GPF. For now
     * use EXCEPTION_ADDR_UNMAPPED unless there is a non-zero error code, as
     * probably the most common cause is a non-canonical address access.
     * Annoyingly, there is no way to get the bad address that was accessed in
     * this case. Most other causes should probably be
     * EXCEPTION_INVALID_INSTRUCTION. Perhaps in future we could decode the
     * instruction to figure out if it's a bad address or not... */
    unhandled_exception(
        frame,
        (frame->err_code != 0) ? EXCEPTION_INVALID_INSTRUCTION : EXCEPTION_ADDR_UNMAPPED);
}

/** Handler for page faults.
 * @param frame         Interrupt stack frame. */
static void pf_exception(frame_t *frame) {
    /* We can't service a page fault while running KDB. */
    if (unlikely(atomic_load(&kdb_running) == 2)) {
        kdb_exception(except_strings[frame->num], frame);
        return;
    }

    ptr_t addr = x86_read_cr2();
    int reason = (frame->err_code & (1 << 0)) ? VM_FAULT_ACCESS : VM_FAULT_UNMAPPED;
    uint32_t access = (cpu_features.xd && frame->err_code & (1<<4))
        ? VM_ACCESS_EXECUTE
        : ((frame->err_code & (1 << 1)) ? VM_ACCESS_WRITE : VM_ACCESS_READ);

    /* Check if a reserved bit fault. This is always fatal. */
    if (frame->err_code & (1 << 3))
        fatal("Reserved bit page fault exception at 0x%zx", addr);

    if (!vm_fault(frame, addr, reason, access))
        unhandled_exception(frame, 0);
}

/** FPU/SIMD Floating-Point Error (#MF/#XM) exception handler.
 * @param frame         Interrupt stack frame. */
static void fpu_exception(frame_t *frame) {
    unsigned code = EXCEPTION_FLOAT_INVALID;

    if (frame_from_user(frame) && x86_fpu_state()) {
        uint16_t err, swd, cwd;
        uint32_t mxcsr;

        if (frame->num == X86_EXCEPTION_XM) {
            /* SSE exceptions are reported via the MXCSR register. Mask bits are
             * at bits 7 through 12. */
            mxcsr = x86_fpu_mxcsr();
            err = mxcsr & ~(mxcsr >> 7);
        } else {
            /* FPU exceptions are reported via the FPU status word, mask bits
             * are in the control word. */
            swd = x86_fpu_swd();
            cwd = x86_fpu_cwd();

            err = swd & ~cwd;
        }

        if (err & X86_FPU_STATUS_DE) {
            code = EXCEPTION_FLOAT_DENORMAL;
        } else if (err & X86_FPU_STATUS_ZE) {
            code = EXCEPTION_FLOAT_DIV_ZERO;
        } else if (err & X86_FPU_STATUS_OE) {
            code = EXCEPTION_FLOAT_OVERFLOW;
        } else if (err & X86_FPU_STATUS_UE) {
            code = EXCEPTION_FLOAT_UNDERFLOW;
        } else if (err & X86_FPU_STATUS_PE) {
            code = EXCEPTION_FLOAT_PRECISION;
        }
    }

    unhandled_exception(frame, code);
}

/** Alignment Check (#AC) exception handler.
 * @param frame         Interrupt stack frame. */
static void ac_exception(frame_t *frame) {
    unhandled_exception(frame, EXCEPTION_INVALID_ALIGNMENT);
}

/** Interrupt handler.
 * @param frame         Interrupt frame. */
void interrupt_handler(frame_t *frame) {
    // TODO: Move this into entry.S, call the handler directly from the entry
    // code.

    bool user = frame->cs & 3;
    if (user) {
        /* Save the user-mode interrupt frame pointer, used by the signal frame
         * setup/restore code. */
        curr_thread->arch.user_frame = frame;
        thread_at_kernel_entry(NULL);
    }

    /* Call the handler. */
    interrupt_table[frame->num](frame);

    /* Preempt if required. */
    if (curr_cpu->should_preempt)
        sched_preempt();

    if (user) {
        thread_at_kernel_exit(NULL, 0);

        /* We must clear the ARCH_THREAD_FRAME_MODIFIED flag if it has been set.
         * This is used in the SYSCALL handler below so that it knows whether to
         * return via the IRET path, but as we're returning using IRET anyway it
         * doesn't matter to us. */
        curr_thread->arch.flags &= ~ARCH_THREAD_FRAME_MODIFIED;
    }
}

/** Initialize the interrupt handler table. */
__init_text void interrupt_init(void) {
    /* Install default handlers. 0-31 are exceptions, 32-47 are IRQs, the rest
     * should be pointed to the unhandled interrupt function. */
    for (size_t i = 0; i < 32; i++)
        interrupt_table[i] = unknown_exception;
    for (size_t i = 32; i < 48; i++)
        interrupt_table[i] = hardware_interrupt;
    for (size_t i = 48; i < array_size(interrupt_table); i++)
        interrupt_table[i] = unknown_interrupt;

    /* Set handlers for faults that require specific handling. */
    interrupt_table[X86_EXCEPTION_DE]  = de_exception;
    interrupt_table[X86_EXCEPTION_DB]  = kdb_db_exception;
    interrupt_table[X86_EXCEPTION_NMI] = nmi_interrupt;
    interrupt_table[X86_EXCEPTION_BP]  = bp_exception;
    interrupt_table[X86_EXCEPTION_UD]  = ud_exception;
    interrupt_table[X86_EXCEPTION_NM]  = nm_exception;
    interrupt_table[X86_EXCEPTION_NP]  = np_exception;
    interrupt_table[X86_EXCEPTION_SS]  = ss_exception;
    interrupt_table[X86_EXCEPTION_GP]  = gp_exception;
    interrupt_table[X86_EXCEPTION_PF]  = pf_exception;
    interrupt_table[X86_EXCEPTION_MF]  = fpu_exception;
    interrupt_table[X86_EXCEPTION_AC]  = ac_exception;
    interrupt_table[X86_EXCEPTION_XM]  = fpu_exception;
}
