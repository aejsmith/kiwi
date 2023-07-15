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
 * @brief               AMD64 Programmable Interrupt Controller code.
 */

#include <arch/io.h>

#include <x86/pic.h>

#include <device/irq.h>

#include <assert.h>
#include <kernel.h>

/** Lock to protect access to PIC. */
static SPINLOCK_DEFINE(pic_lock);

/** IRQ masks - disable all by default, apart from IRQ2 (cascade). */
static uint8_t pic_mask_master = 0xfb;
static uint8_t pic_mask_slave = 0xff;

/** Level-triggered interrupts. */
static uint16_t pic_level_triggered;

static void pic_eoi(uint32_t num) {
    if (num >= 8)
        out8(PIC_SLAVE_COMMAND, PIC_COMMAND_EOI);

    /* Must always send the EOI to the master controller. */
    out8(PIC_MASTER_COMMAND, PIC_COMMAND_EOI);
}

static void pic_disable_locked(uint32_t num) {
    if (num >= 8) {
        pic_mask_slave |= (1<<(num - 8));
        out8(PIC_SLAVE_DATA, pic_mask_slave);
    } else {
        pic_mask_master |= (1<<num);
        out8(PIC_MASTER_DATA, pic_mask_master);
    }
}

static bool pic_pre_handle(uint32_t num) {
    assert(num < 16);

    spinlock_lock(&pic_lock);

    bool handle = true;

    /* Check for spurious IRQs. */
    if (num == 7) {
        /* Read the In-Service Register, check the high bit. */
        out8(0x23, 3);
        if ((in8(0x20) & 0x80) == 0) {
            kprintf(LOG_DEBUG, "pic: spurious IRQ7 (master), ignoring...\n");
            handle = false;
        }
    } else if (num == 15) {
        /* Read the In-Service Register, check the high bit. */
        out8(0xa3, 3);
        if ((in8(0xa0) & 0x80) == 0) {
            kprintf(LOG_DEBUG, "pic: spurious IRQ15 (slave), ignoring...\n");
            handle = false;
        }
    }

    /* Edge-triggered interrupts must be acked before we handle. */
    if (handle && !(pic_level_triggered & (1 << num)))
        pic_eoi(num);

    spinlock_unlock(&pic_lock);
    return handle;
}

static void pic_post_handle(uint32_t num, bool disable) {
    spinlock_lock(&pic_lock);

    if (disable)
        pic_disable_locked(num);

    /* Level-triggered interrupts must be acked once all handlers have been run. */
    if (pic_level_triggered & (1 << num))
        pic_eoi(num);

    spinlock_unlock(&pic_lock);
}

static irq_mode_t pic_mode(uint32_t num) {
    return (pic_level_triggered & (1 << num));
}

static void pic_enable(uint32_t num) {
    assert(num < 16);

    spinlock_lock(&pic_lock);

    if (num >= 8) {
        pic_mask_slave &= ~(1 << (num - 8));
        out8(PIC_SLAVE_DATA, pic_mask_slave);
    } else {
        pic_mask_master &= ~(1<<num);
        out8(PIC_MASTER_DATA, pic_mask_master);
    }

    spinlock_unlock(&pic_lock);
}

static void pic_disable(uint32_t num) {
    assert(num < 16);

    spinlock_lock(&pic_lock);
    pic_disable_locked(num);
    spinlock_unlock(&pic_lock);
}

static irq_controller_t pic_irq_controller = {
    .pre_handle  = pic_pre_handle,
    .post_handle = pic_post_handle,
    .mode        = pic_mode,
    .enable      = pic_enable,
    .disable     = pic_disable,
};

static __init_text void pic_init(void) {
    /* Send an initialization command to both PICs (ICW1). */
    out8(PIC_MASTER_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    out8(PIC_SLAVE_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);

    /* Set the interrupt vectors to use (ICW2). */
    out8(PIC_MASTER_DATA, 32);
    out8(PIC_SLAVE_DATA, 32 + 8);

    /* Set how the PICs are connected to each other (ICW3). */
    out8(PIC_MASTER_DATA, 0x04);
    out8(PIC_SLAVE_DATA, 0x02);

    /* Set other behaviour flags (ICW4). */
    out8(PIC_MASTER_DATA, PIC_ICW4_8086);
    out8(PIC_SLAVE_DATA, PIC_ICW4_8086);

    /* Set initial IRQ masks. */
    out8(PIC_MASTER_DATA, pic_mask_master);
    out8(PIC_SLAVE_DATA, pic_mask_slave);

    /* Get the trigger modes. */
    pic_level_triggered = (in8(PIC_SLAVE_ELCR) << 8) | in8(PIC_MASTER_ELCR);

    /* TODO: This will change once we support IOAPIC. */
    root_irq_domain = irq_domain_create(PIC_IRQ_COUNT, &pic_irq_controller);
}

INITCALL_TYPE(pic_init, INITCALL_TYPE_IRQ);
