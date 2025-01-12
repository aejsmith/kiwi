/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 thread functions.
 */

#include <arch/cpu.h>
#include <arch/frame.h>
#include <arch/stack.h>

#include <arm64/cpu.h>

#include <lib/string.h>

#include <mm/safe.h>

#include <proc/thread.h>

#include <assert.h>
#include <status.h>

static_assert(offsetof(arch_thread_t, user_frame) == ARCH_THREAD_OFF_user_frame, "Incorrect thread offset definitions");

extern void arm64_context_switch(ptr_t new_sp, ptr_t *_old_sp);
extern void arm64_context_restore(ptr_t new_sp);

/** Initialize ARM64-specific thread data.
 * @param thread        Thread to initialize. */
void arch_thread_init(thread_t *thread) {
    thread->arch.parent = thread;

    /* Initialize the kernel stack. */
    ptr_t entry = (ptr_t)thread_trampoline;
    unsigned long *sp = (unsigned long *)((ptr_t)thread->kstack + KSTACK_SIZE);
    *--sp = entry;  /* LR/X30. */
    *--sp = 0;      /* FP/X29. */
    *--sp = 0;      /* X28. */
    *--sp = 0;      /* X27. */
    *--sp = 0;      /* X26. */
    *--sp = 0;      /* X25. */
    *--sp = 0;      /* X24. */
    *--sp = 0;      /* X23. */
    *--sp = 0;      /* X22. */
    *--sp = 0;      /* X21. */
    *--sp = 0;      /* X20. */
    *--sp = 0;      /* X19. */

    /* Save the stack pointer for arch_thread_switch(). */
    thread->arch.saved_sp = (ptr_t)sp;
}

/** Clean up ARM64-specific thread data.
 * @param thread        Thread to clean up. */
void arch_thread_destroy(thread_t *thread) {
    /* Nothing happens. */
}

/** Clone the current thread.
 * @param thread        New thread to clone into.
 * @param frame         Frame to prepare for new thread to enter user mode with
 *                      arch_thread_user_enter(). */
void arch_thread_clone(thread_t *thread, frame_t *frame) {
    fatal_todo();
}

/** Switch to another thread.
 * @param thread        Thread to switch to.
 * @param prev          Thread that was previously running. */
void arch_thread_switch(thread_t *thread, thread_t *prev) {
    // TODO: FPU state.

    /* Store the current CPU pointer, then set TPIDR_EL1 to point to the current
     * thread. The curr_cpu load will come from the previous thread's data. */
    thread->arch.cpu = arch_curr_cpu_volatile();
    arm64_write_sysreg(tpidr_el1, &thread->arch);
    arm64_isb();

    /* Switch to the new context. */
    if (likely(prev)) {
        arm64_context_switch(thread->arch.saved_sp, &prev->arch.saved_sp);
    } else {
        /* Initial thread switch, don't have a previous thread. */
        arm64_context_restore(thread->arch.saved_sp);
    }
}

/** Set the TLS address for the current thread.
 * @param addr          TLS address. */
void arch_thread_set_tls_addr(ptr_t addr) {
    fatal_todo();
}

/** Prepare an interrupt frame to enter user mode.
 * @param frame         Frame to prepare.
 * @param entry         Entry function.
 * @param sp            Stack pointer.
 * @param arg           First argument to function. */
void arch_thread_user_setup(frame_t *frame, ptr_t entry, ptr_t sp, ptr_t arg) {
    assert((sp % 16) == 0);

    /* Clear out the frame to zero all GPRs. */
    memset(frame, 0, sizeof(*frame));

    frame->ip   = entry;
    frame->sp   = sp;
    frame->x0   = arg;
    frame->spsr = ARM64_SPSR_MODE_EL0T;
}

/** Prepare to execute a user mode interrupt.
 * @param interrupt     Interrupt to execute.
 * @param ipl           Previous IPL.
 * @return              Status code describing result of the operation. */
status_t arch_thread_interrupt_setup(thread_interrupt_t *interrupt, uint32_t ipl) {
    fatal_todo();
}

/** Restore previous state after returning from a user mode interrupt.
 * @param _ipl          Where to store previous IPL.
 * @return              Status code describing result of the operation. */
status_t arch_thread_interrupt_restore(uint32_t *_ipl) {
    fatal_todo();
}

/** Log a user backtrrace for the current thread. */
void arch_thread_backtrace(void (*cb)(ptr_t)) {
    frame_t *frame = curr_thread->arch.user_frame;
    assert(frame_from_user(frame));

    cb(frame->ip);

    const size_t max_depth = 8;
    ptr_t fp = frame->x29;
    for(size_t i = 0; fp && i < max_depth; i++) {
        stack_frame_t frame;
        status_t ret = memcpy_from_user(&frame, (void *)fp, sizeof(frame));
        if (ret != STATUS_SUCCESS)
            break;

        if (frame.addr)
            cb(frame.addr);

        fp = frame.next;
    }
}
