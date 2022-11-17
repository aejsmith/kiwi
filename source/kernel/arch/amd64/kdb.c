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
 * @brief               AMD64 kernel debugger functions.
 */

#include <arch/frame.h>
#include <arch/kdb.h>
#include <arch/stack.h>

#include <x86/cpu.h>
#include <x86/interrupt.h>
#include <x86/lapic.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <proc/thread.h>

#include <cpu.h>
#include <kdb.h>

/** Structure containing details of a breakpoint. */
typedef struct breakpoint {
    unsigned long dr7;          /**< Value to OR into DR7. */
    ptr_t addr;                 /**< Address of the breakpoint. */
} breakpoint_t;

extern void kdb_db_exception(frame_t *frame);

/** Breakpoint/watchpoint tracking structures. */
static breakpoint_t kdb_breakpoints[KDB_BREAKPOINT_COUNT];

/** Set breakpoint settings in the debug registers. */
static inline void setup_debug_regs(void) {
    unsigned long dr7 = 0;

    x86_write_dr0(kdb_breakpoints[0].addr);
    dr7 |= kdb_breakpoints[0].dr7;
    x86_write_dr1(kdb_breakpoints[1].addr);
    dr7 |= kdb_breakpoints[1].dr7;
    x86_write_dr2(kdb_breakpoints[2].addr);
    dr7 |= kdb_breakpoints[2].dr7;
    x86_write_dr3(kdb_breakpoints[3].addr);
    dr7 |= kdb_breakpoints[3].dr7;

    x86_write_dr7(dr7);
}

/** Enter the debugger.
 * @param reason        Reason for entry to the debugger.
 * @param frame         Interrupt frame.
 * @param index         Index of breakpoint or watchpoint that caused entry. */
static void kdb_enter_internal(kdb_reason_t reason, frame_t *frame, unsigned index) {
    /* Disable breakpoints while KDB is running. */
    x86_write_dr7(0);

    if (kdb_main(reason, frame, index) == KDB_STEP) {
        frame->flags |= X86_FLAGS_TF;
    } else {
        frame->flags &= ~X86_FLAGS_TF;
    }

    /* Work out a new Debug Control Register value. */
    setup_debug_regs();
}

/** Debug exception handler.
 * @param frame         Interrupt frame. */
void kdb_db_exception(frame_t *frame) {
    kdb_reason_t reason = KDB_REASON_USER;
    unsigned i = 0;

    /* Work out the reason. */
    unsigned long dr6 = x86_read_dr6();
    if (!(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD | X86_DR6_BS | X86_DR6_BT))) {
        /* No bits set, assume this came from from kdb_enter(), in which case
         * the reason will be in AX. */
        reason = (kdb_reason_t)frame->ax;
    } else {
        if (dr6 & X86_DR6_BS) {
            reason = KDB_REASON_STEP;
        } else if (dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3)) {
            for (; i < array_size(kdb_breakpoints); i++) {
                if (frame->ip == kdb_breakpoints[i].addr) {
                    reason = KDB_REASON_BREAK;
                    break;
                } else if (dr6 & (1ul << i)) {
                    reason = KDB_REASON_WATCH;
                    break;
                }
            }
        }
    }

    kdb_enter_internal(reason, frame, i);

    /* Clear the Debug Status Register (DR6). */
    x86_write_dr6(0);

    /* Set the resume flag if resuming from a breakpoint so that we do not
     * immediately break again. */
    if (reason == KDB_REASON_BREAK)
        frame->flags |= X86_FLAGS_RF;
}

/** Enter the kernel debugger.
 * @param reason        Reason for entry.
 * @param frame         Interrupt frame that caused entry (if NULL one will be
 *                      generated). */
void kdb_enter(kdb_reason_t reason, frame_t *frame) {
    if (frame) {
        kdb_enter_internal(reason, frame, 0);
    } else {
        /* Raise a debug interrupt so we can get into the debugger with an
         * interrupt frame. Store the entry reason in EAX, which will be picked
         * up in the #DB handler above. */
        __asm__ volatile("int $1" :: "a"((unsigned long)reason));
        return;
    }
}

/** Install a kernel breakpoint.
 * @param addr          Address of the breakpoint.
 * @return              Index of added breakpoint, or -1 if none available. */
int arch_kdb_install_breakpoint(ptr_t addr) {
    for (size_t i = 0; i < array_size(kdb_breakpoints); i++) {
        if (kdb_breakpoints[i].dr7)
            continue;

        kdb_breakpoints[i].dr7 = (1<<(1+(i*2)));
        kdb_breakpoints[i].addr = addr;
        return i;
    }

    kdb_printf("No free breakpoint slots.\n");
    return -1;
}

/** Install a kernel watchpoint.
 * @param addr          Address of the watchpoint.
 * @param size          Size of accesses to trigger on.
 * @param rw            Whether to trigger on reads and writes, rather than
 *                      just writes.
 * @return              Index of added watchpoint, or -1 if none available. */
int arch_kdb_install_watchpoint(ptr_t addr, size_t size, bool rw) {
    for (size_t i = 0; i < array_size(kdb_breakpoints); i++) {
        if (kdb_breakpoints[i].dr7)
            continue;

        /* Set the global enable bit for the breakpoint. */
        unsigned long dr7 = (1 << (1 + (i * 2)));

        /* Set the condition. */
        dr7 |= (1 << (16 + (i * 4)));
        if (rw)
            dr7 |= (1 << (17 + (i * 4)));

        /* Set the size. */
        switch (size) {
            case 1:
                break;
            case 4:
                dr7 |= (1 << (19 + (i * 4)));
            case 2:
                dr7 |= (1 << (18 + (i * 4)));
                break;
            case 8:
                dr7 |= (1 << (19 + (i * 4)));
                break;
            default:
                kdb_printf("Invalid size.\n");
                return -1;
        }

        kdb_breakpoints[i].dr7 = dr7;
        kdb_breakpoints[i].addr = addr;
        return i;
    }

    kdb_printf("No free breakpoint slots.\n");
    return -1;
}

/** Remove a kernel breakpoint.
 * @param index         Index of breakpoint to remove.
 * @return              Whether the breakpoint existed. */
bool arch_kdb_remove_breakpoint(unsigned index) {
    if (index >= array_size(kdb_breakpoints) ||
        !kdb_breakpoints[index].dr7 ||
        (kdb_breakpoints[index].dr7 & ~(1 << (1 + (index * 2)))))
    {
        kdb_printf("Breakpoint ID %u invalid.\n", index);
        return false;
    }

    kdb_breakpoints[index].dr7 = 0;
    return true;
}

/** Remove a kernel watchpoint.
 * @param index     Index of watchpoint to remove.
 * @return          Whether the breakpoint existed. */
bool arch_kdb_remove_watchpoint(unsigned index) {
    if (index >= array_size(kdb_breakpoints) ||
        !kdb_breakpoints[index].dr7 ||
        !(kdb_breakpoints[index].dr7 & ~(1 << (1 + (index * 2)))))
    {
        kdb_printf("Watchpoint ID %u invalid.\n", index);
        return false;
    }

    kdb_breakpoints[index].dr7 = 0;
    return true;
}

/** Get details of a breakpoint.
 * @param index         Index of breakpoint.
 * @param _addr         Where to store address of breakpoint.
 * @return              Whether the breakpoint existed. */
bool arch_kdb_get_breakpoint(unsigned index, ptr_t *_addr) {
    if (!kdb_breakpoints[index].dr7 || (kdb_breakpoints[index].dr7 & ~(1 << (1 + (index * 2)))))
        return false;

    *_addr = kdb_breakpoints[index].addr;
    return true;
}

/** Get details of a watchpoint.
 * @param index         Index of watchpoint.
 * @param _addr         Where to store address of watchpoint.
 * @param _size         Where to store size of watchpoint.
 * @param _rw           Where to store read-write property.
 * @return              Whether the watchpoint existed. */
bool arch_kdb_get_watchpoint(unsigned index, ptr_t *_addr, size_t *_size, bool *_rw) {
    if (!kdb_breakpoints[index].dr7 || !(kdb_breakpoints[index].dr7 & ~(1 << (1 + (index * 2)))))
        return false;

    /* Work out the size. */
    switch ((kdb_breakpoints[index].dr7 >> (18 + (index * 4))) & 0x3) {
        case 0:
            *_size = 1;
            break;
        case 1:
            *_size = 2;
            break;
        case 2:
            *_size = 8;
            break;
        case 3:
            *_size = 4;
            break;
    }

    /* Get RW property. */
    if (kdb_breakpoints[index].dr7 & (1 << (17 + (index * 4)))) {
        *_rw = true;
    } else {
        *_rw = false;
    }

    *_addr = kdb_breakpoints[index].addr;
    return true;
}

/** Check if an address is within a stack.
 * @param addr          Address to check.
 * @param stack         Stack to check. */
static inline ptr_t is_in_stack(ptr_t addr, void *stack) {
    return addr >= (ptr_t)stack && addr < ((ptr_t)stack + KSTACK_SIZE);
}

/** Check if an address lies within the kernel stack.
 * @param thread        Thread to check for.
 * @param addr          Address to check.
 * @return              Whether the address is in the kernel stack. */
static bool is_kstack_address(thread_t *thread, ptr_t addr) {
    return
        is_in_stack(addr, thread->kstack) ||
        is_in_stack(addr, curr_cpu->arch.double_fault_stack);
}

/** Perform a backtrace.
 * @param thread        Thread to trace. If NULL, use the current frame.
 * @param cb            Backtrace callback. */
void arch_kdb_backtrace(thread_t *thread, kdb_backtrace_cb_t cb) {
    /* Get the stack frame. */
    ptr_t bp;
    if (thread) {
        unsigned long *sp = (unsigned long *)thread->arch.saved_rsp;
        bp = sp[5];
    } else {
        bp = curr_kdb_frame->bp;
    }

    while (bp) {
        /* Don't want to go off into user memory if this isn't the current
         * process. */
        if (thread && thread->owner != curr_thread->owner) {
            if (!is_kstack_address(thread, bp))
                break;
        }

        stack_frame_t *frame = (stack_frame_t *)bp;

        if (frame->addr)
            cb(frame->addr);

        bp = frame->next;
    }
}

/** Get the value of a register.
 * @param name          Name of register.
 * @param len           Length of register name.
 * @param _reg          Location to store register value in.
 * @return              Whether the register name was valid. */
bool arch_kdb_register_value(const char *name, size_t len, unsigned long *_reg) {
    #define KDB_REGISTER_CHECK(rn, rl, rv)  \
        if (len == (rl) && strncasecmp(name, (rn), (rl)) == 0) { \
            *_reg = (rv); \
            return true; \
        }

    KDB_REGISTER_CHECK("cs", 2, curr_kdb_frame->cs);
    KDB_REGISTER_CHECK("num", 6, curr_kdb_frame->num);
    KDB_REGISTER_CHECK("err_code", 8, curr_kdb_frame->err_code);
    KDB_REGISTER_CHECK("r15", 3, curr_kdb_frame->r15);
    KDB_REGISTER_CHECK("r14", 3, curr_kdb_frame->r14);
    KDB_REGISTER_CHECK("r13", 3, curr_kdb_frame->r13);
    KDB_REGISTER_CHECK("r12", 3, curr_kdb_frame->r12);
    KDB_REGISTER_CHECK("r11", 3, curr_kdb_frame->r11);
    KDB_REGISTER_CHECK("r10", 3, curr_kdb_frame->r10);
    KDB_REGISTER_CHECK("r9", 2, curr_kdb_frame->r9);
    KDB_REGISTER_CHECK("r8", 2, curr_kdb_frame->r8);
    KDB_REGISTER_CHECK("rbp", 3, curr_kdb_frame->bp);
    KDB_REGISTER_CHECK("rsi", 3, curr_kdb_frame->si);
    KDB_REGISTER_CHECK("rdi", 3, curr_kdb_frame->di);
    KDB_REGISTER_CHECK("rdx", 3, curr_kdb_frame->dx);
    KDB_REGISTER_CHECK("rcx", 3, curr_kdb_frame->cx);
    KDB_REGISTER_CHECK("rbx", 3, curr_kdb_frame->bx);
    KDB_REGISTER_CHECK("rax", 3, curr_kdb_frame->ax);
    KDB_REGISTER_CHECK("rip", 3, curr_kdb_frame->ip);
    KDB_REGISTER_CHECK("rflags", 6, curr_kdb_frame->flags);
    KDB_REGISTER_CHECK("rsp", 3, curr_kdb_frame->sp);
    KDB_REGISTER_CHECK("ss", 2, curr_kdb_frame->ss);
    return false;
}

/** Print out all registers. */
void arch_kdb_dump_registers(bool user) {
    frame_t *frame = NULL;

    if (user) {
        thread_t *thread = curr_thread;
        if (thread) {
            frame = thread->arch.user_frame;
            if (thread->arch.user_frame && frame_from_user(thread->arch.user_frame))
                frame = thread->arch.user_frame;
        }

        if (!frame) {
            kdb_printf("No user frame available\n");
            return;
        }
    } else {
        frame = curr_kdb_frame;
    }

    kdb_printf("RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx\n", frame->ax, frame->bx, frame->cx);
    kdb_printf("RDX: 0x%016lx  RDI: 0x%016lx  RSI: 0x%016lx\n", frame->dx, frame->di, frame->si);
    kdb_printf("RBP: 0x%016lx  R8:  0x%016lx  R9:  0x%016lx\n", frame->bp, frame->r8, frame->r9);
    kdb_printf("R10: 0x%016lx  R11: 0x%016lx  R12: 0x%016lx\n", frame->r10, frame->r11, frame->r12);
    kdb_printf("R13: 0x%016lx  R14: 0x%016lx  R15: 0x%016lx\n", frame->r13, frame->r14, frame->r15);
    kdb_printf("RIP: 0x%016lx  RSP: 0x%016lx  RFL: 0x%016lx\n", frame->ip, frame->sp, frame->flags);
    kdb_printf("CS:  0x%04lx  SS: 0x%04lx\n", frame->cs, frame->ss);

    if (!user) {
        switch (frame->num) {
            case X86_EXCEPTION_PF:
                kdb_printf(
                    "EC:  0x%04lx (%s/%s%s%s)\n", frame->err_code,
                    (frame->err_code & (1 << 0)) ? "protection" : "not-present",
                    (frame->err_code & (1 << 1)) ? "write" : "read",
                    (frame->err_code & (1 << 3)) ? "/reserved-bit" : "",
                    (frame->err_code & (1 << 4)) ? "/execute" : "");
                kdb_printf("CR2: 0x%016lx\n", x86_read_cr2());
                break;
            case X86_EXCEPTION_DF:
            case X86_EXCEPTION_TS:
            case X86_EXCEPTION_NP:
            case X86_EXCEPTION_SS:
            case X86_EXCEPTION_GP:
            case X86_EXCEPTION_AC:
                kdb_printf("EC:  0x%04lx\n", frame->err_code);
                break;
        }
    }
}

/** Trap all other CPUs to wait for KDB to exit. */
void arch_kdb_trap_cpus(void) {
    if (cpu_count > 1) {
        /*
         * The NMI handler checks kdb_running and spins until it is 0.
         *
         * It is not safe to use LAPIC_IPI_DEST_ALL here in case a CPU is not
         * currently running - this will cause a triple fault.
         */
        for (size_t i = 0; i <= highest_cpu_id; i++) {
            if (cpus[i] && cpus[i] != curr_cpu && cpus[i]->state == CPU_RUNNING)
                lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)i, LAPIC_IPI_NMI, 0);
        }
    }
}

/** Register architecture-specific KDB commands. */
__init_text void arch_kdb_init(void) {
    /* Nothing happens. */
}
