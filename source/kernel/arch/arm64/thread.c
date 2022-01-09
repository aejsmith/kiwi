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
 * @brief               ARM64 thread functions.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <proc/thread.h>

/** Initialize ARM64-specific thread data.
 * @param thread        Thread to initialize. */
void arch_thread_init(thread_t *thread) {
    thread->arch.parent = thread;

    /* Initialize the kernel stack. */
    ptr_t entry = (ptr_t)thread_trampoline;
    unsigned long *sp = (unsigned long *)((ptr_t)thread->kstack + KSTACK_SIZE);
    *--sp = entry;  /* LR. */
    *--sp = 0;      /* FP. */
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
    // TODO: Point TPIDR to arch_thread.
    fatal_todo();
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
    fatal_todo();
}

/** Prepare to execute a user mode interrupt.
 * @param interrupt     Interrupt to execute.
 * @param ipl           Previous IPL.
 * @return              Status code describing result of the operation. */
status_t arch_thread_interrupt_setup(thread_interrupt_t *interrupt, unsigned ipl) {
    fatal_todo();
}

/** Restore previous state after returning from a user mode interrupt.
 * @param _ipl          Where to store previous IPL.
 * @return              Status code describing result of the operation. */
status_t arch_thread_interrupt_restore(unsigned *_ipl) {
    fatal_todo();
}
