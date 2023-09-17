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
 * @brief               ARM64 exception handling.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>
#include <arm64/kdb.h>

#include <mm/vm.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <kdb.h>
#include <kernel.h>

enum {
    EC_SVC_AARCH64              = 0b010101,
    EC_INSTRUCTION_ABORT_EL0    = 0b100000,
    EC_INSTRUCTION_ABORT_EL1    = 0b100001,
    EC_DATA_ABORT_EL0           = 0b100100,
    EC_DATA_ABORT_EL1           = 0b100101,
    EC_BRK_AARCH64              = 0b111100,
};

extern uint8_t arm64_exception_vectors[];

static arm64_irq_handler_t arm64_irq_handler_func;
static void *arm64_irq_handler_private;

static void common_entry(frame_t *frame) {
    if (frame_from_user(frame))
        thread_at_kernel_entry(NULL);
}

static void common_exit(frame_t *frame) {
    if (frame_from_user(frame))
        thread_at_kernel_exit(NULL, 0);
}

/** Sets the hardware IRQ handler. */
void arm64_set_irq_handler(arm64_irq_handler_t handler, void *private) {
    if (arm64_irq_handler_func)
        fatal("Multiple IRQ handlers installed");

    arm64_irq_handler_func    = handler;
    arm64_irq_handler_private = private;
}

/** Handle an IRQ. */
void arm64_irq_handler(frame_t *frame) {
    common_entry(frame);

    if (!arm64_irq_handler_func)
        fatal("Received IRQ without registered IRQ handler");

    arm64_irq_handler_func(arm64_irq_handler_private, frame);

    common_exit(frame);
}

static const char *exception_class_to_string(uint64_t class) {
    switch (class) {
        case EC_SVC_AARCH64:            return "SVC (AArch64)";
        case EC_INSTRUCTION_ABORT_EL0:  return "Instruction Abort (EL0)";
        case EC_INSTRUCTION_ABORT_EL1:  return "Instruction Abort (EL1)";
        case EC_DATA_ABORT_EL0:         return "Data Abort (EL0)";
        case EC_DATA_ABORT_EL1:         return "Data Abort (EL1)";
        case EC_BRK_AARCH64:            return "BRK (AArch64)";
        default:                        return "Unknown";
    }
}

static void unhandled_exception(frame_t *frame, uint64_t esr) {
    uint64_t class     = ARM64_ESR_EC(esr);
    const char *string = exception_class_to_string(class);

    if (atomic_load(&kdb_running) == 2) {
        kdb_exception(string, frame);
    } else {
        // TODO: User exceptions.
        fatal_etc(
            frame, "Unhandled %s mode exception %lu (%s)",
            (frame_from_user(frame)) ? "user" : "kernel", class, string);
    }
}

static bool mmu_exception(frame_t *frame, uint64_t esr, bool instruction) {
    /* We can't handle an MMU fault while running KDB. */
    if (unlikely(atomic_load(&kdb_running) == 2))
        return false;

    uint64_t iss  = ARM64_ESR_ISS(esr);
    uint64_t far  = arm64_read_sysreg(far_el1);

    int reason      = -1;
    uint32_t access = 0;

    /* DFSC/IFSC are mostly the same. Mask out the low 2 bits that specify the
     * table level, we don't care about this. */
    uint64_t fsc = iss & 0x3c;
    switch (fsc) {
        case 0b000100:
            /* Translation fault. */
            reason = VM_FAULT_UNMAPPED;
            break;
        case 0b001100:
            /* Permission fault. */
            reason = VM_FAULT_ACCESS;
            break;
    }

    if (instruction) {
        access = VM_ACCESS_EXECUTE;
    } else {
        /* Check WnR bit for access type. */
        access = (iss & (1 << 6)) ? VM_ACCESS_WRITE : VM_ACCESS_READ;
    }

    if (reason != -1) {
        return vm_fault(frame, far, reason, access);
    } else {
        // TODO: Some unhandled exceptions should probably be fatal even from
        // user mode?
        return false;
    }
}

/** Handle a synchronous exception. */
void arm64_sync_exception_handler(frame_t *frame) {
    common_entry(frame);

    uint64_t esr   = arm64_read_sysreg(esr_el1);
    uint64_t class = ARM64_ESR_EC(esr);

    bool handled = false;

    switch (class) {
        case EC_INSTRUCTION_ABORT_EL0:
        case EC_INSTRUCTION_ABORT_EL1:
            handled = mmu_exception(frame, esr, true);
            break;
        case EC_DATA_ABORT_EL0:
        case EC_DATA_ABORT_EL1:
            handled = mmu_exception(frame, esr, false);
            break;
        case EC_BRK_AARCH64:
            // TODO: Should not enter KDB if this came from EL0.
            arm64_kdb_brk_handler(frame);
            handled = true;
            break;
    }

    if (!handled)
        unhandled_exception(frame, esr);

    common_exit(frame);
}

/** Unhandled exception. */
void arm64_unhandled_exception_handler(frame_t *frame) {
    uint64_t esr   = arm64_read_sysreg(esr_el1);
    uint64_t class = ARM64_ESR_EC(esr);

    fatal_etc(frame, "Unhandled CPU exception (ESR = 0x%" PRIx64 " class = 0x%" PRIx64 ")", esr, class);
}

/** Set up exception handling. */
__init_text void arm64_exception_init(void) {
    /* Ensure we run exceptions with current EL SP. */
    arm64_write_sysreg(spsel, 1);

    /* Install exception vectors. */
    arm64_write_sysreg(vbar_el1, (ptr_t)arm64_exception_vectors);
}
