/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               ARM64 kernel debugger functions.
 */

#include <arch/frame.h>
#include <arch/kdb.h>

#include <arm64/cpu.h>

#include <proc/thread.h>

#include <cpu.h>
#include <kdb.h>

/** Structure containing a stack frame. */
typedef struct stack_frame {
    ptr_t next;                 /**< Address of next stack frame. */
    ptr_t addr;                 /**< Function return address. */
} stack_frame_t;

/** Install a kernel breakpoint.
 * @param addr          Address of the breakpoint.
 * @return              Index of added breakpoint, or -1 if none available. */
int arch_kdb_install_breakpoint(ptr_t addr) {
    kdb_printf("TODO");
    return -1;
}

/** Install a kernel watchpoint.
 * @param addr          Address of the watchpoint.
 * @param size          Size of accesses to trigger on.
 * @param rw            Whether to trigger on reads and writes, rather than
 *                      just writes.
 * @return              Index of added watchpoint, or -1 if none available. */
int arch_kdb_install_watchpoint(ptr_t addr, size_t size, bool rw) {
    kdb_printf("TODO");
    return -1;
}

/** Remove a kernel breakpoint.
 * @param index         Index of breakpoint to remove.
 * @return              Whether the breakpoint existed. */
bool arch_kdb_remove_breakpoint(unsigned index) {
    kdb_printf("TODO");
    return false;
}

/** Remove a kernel watchpoint.
 * @param index     Index of watchpoint to remove.
 * @return          Whether the breakpoint existed. */
bool arch_kdb_remove_watchpoint(unsigned index) {
    kdb_printf("TODO");
    return false;
}

/** Get details of a breakpoint.
 * @param index         Index of breakpoint.
 * @param _addr         Where to store address of breakpoint.
 * @return              Whether the breakpoint existed. */
bool arch_kdb_get_breakpoint(unsigned index, ptr_t *_addr) {
    return false;
}

/** Get details of a watchpoint.
 * @param index         Index of watchpoint.
 * @param _addr         Where to store address of watchpoint.
 * @param _size         Where to store size of watchpoint.
 * @param _rw           Where to store read-write property.
 * @return              Whether the watchpoint existed. */
bool arch_kdb_get_watchpoint(unsigned index, ptr_t *_addr, size_t *_size, bool *_rw) {
    return false;
}

/** Perform a backtrace.
 * @param thread        Thread to trace. If NULL, use the current frame.
 * @param cb            Backtrace callback. */
void arch_kdb_backtrace(thread_t *thread, kdb_backtrace_cb_t cb) {
    ptr_t fp;
    if (thread) {
        kdb_printf("Thread backtrace TODO\n");
        return;
    } else {
        fp = curr_kdb_frame->x29;
    }

    while (fp) {
        stack_frame_t *frame = (stack_frame_t *)fp;

        if (frame->addr)
            cb(frame->addr);

        fp = frame->next;
    }
}

/** Get the value of a register.
 * @param name          Name of register.
 * @param len           Length of register name.
 * @param _reg          Location to store register value in.
 * @return              Whether the register name was valid. */
bool arch_kdb_register_value(const char *name, size_t len, unsigned long *_reg) {
    kdb_printf("TODO");
    return false;
}

/** Print out all registers. */
void arch_kdb_dump_registers(void) {
    frame_t *frame = curr_kdb_frame;

    unsigned long far = arm64_read_sysreg(far_el1);
    unsigned long esr = arm64_read_sysreg(esr_el1);

    kdb_printf(
        "X0:   0x%016lx  X1:  0x%016lx  X2:  0x%016lx\n"
        "X3:   0x%016lx  X4:  0x%016lx  X5:  0x%016lx\n"
        "X6:   0x%016lx  X7:  0x%016lx  X8:  0x%016lx\n"
        "X9:   0x%016lx  X10: 0x%016lx  X11: 0x%016lx\n"
        "X12:  0x%016lx  X13: 0x%016lx  X14: 0x%016lx\n"
        "X15:  0x%016lx  X16: 0x%016lx  X17: 0x%016lx\n"
        "X18:  0x%016lx  X19: 0x%016lx  X20: 0x%016lx\n"
        "X21:  0x%016lx  X22: 0x%016lx  X23: 0x%016lx\n"
        "X24:  0x%016lx  X25: 0x%016lx  X26: 0x%016lx\n"
        "X27:  0x%016lx  X28: 0x%016lx  X29: 0x%016lx\n"
        "X30:  0x%016lx  SP:  0x%016lx  ELR: 0x%016lx\n"
        "SPSR: 0x%016lx  FAR: 0x%016lx  ESR: 0x%08lx\n",
        frame->x0,  frame->x1,  frame->x2,
        frame->x3,  frame->x4,  frame->x5,
        frame->x6,  frame->x7,  frame->x8,
        frame->x9,  frame->x10, frame->x11,
        frame->x12, frame->x13, frame->x14,
        frame->x15, frame->x16, frame->x17,
        frame->x18, frame->x19, frame->x20,
        frame->x21, frame->x22, frame->x23,
        frame->x24, frame->x25, frame->x26,
        frame->x27, frame->x28, frame->x29,
        frame->x30, frame->sp,  frame->ip,
        frame->spsr, far, esr);
}

/** Trap all other CPUs to wait for KDB to exit. */
void arch_kdb_trap_cpus(void) {
    if (cpu_count > 1) {
        kdb_printf("TODO");
    }
}

/** Enter the debugger. */
static void kdb_enter_internal(kdb_reason_t reason, frame_t *frame, unsigned index) {
    // TODO: See AMD64 for breakpoint control...

    kdb_main(reason, frame, index);
}

/** Enter the kernel debugger.
 * @param reason        Reason for entry.
 * @param frame         Interrupt frame that caused entry (if NULL one will be
 *                      generated). */
void kdb_enter(kdb_reason_t reason, frame_t *frame) {
    if (frame) {
        kdb_enter_internal(reason, frame, 0);
    } else {
        kdb_printf("KDB TODO!");
        arch_cpu_halt();
    }
}

/** Register architecture-specific KDB commands. */
__init_text void arch_kdb_init(void) {
    /* Nothing happens. */
}
