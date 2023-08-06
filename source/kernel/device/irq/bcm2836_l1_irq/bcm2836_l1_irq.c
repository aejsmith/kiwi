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
 * @brief               BCM2836 L1 IRQ controller driver.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>

#include <device/bus/dt.h>
#include <device/irq.h>

#include <mm/malloc.h>

#include <assert.h>
#include <cpu.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** Register definitions. */
enum {
    /** Timer interrupt control (per-core). */
    BCM2836_L1_REG_TIMER_INT_CONTROL0   = 0x40,

    /** Mailbox interrupt control (per-core). */
    BCM2836_L1_REG_MAILBOX_INT_CONTROL0 = 0x50,

    /** Interrupt pending (per-core). */
    BCM2836_L1_REG_PENDING0             = 0x60,
};

/** IRQ numbers. */
enum {
    BCM2836_L1_IRQ_CNTPSIRQ     = 0,
    BCM2836_L1_IRQ_CNTPNSIRQ    = 1,
    BCM2836_L1_IRQ_CNTHPIRQ     = 2,
    BCM2836_L1_IRQ_CNTVIRQ      = 3,
    BCM2836_L1_IRQ_MAILBOX0     = 4,
    BCM2836_L1_IRQ_MAILBOX1     = 5,
    BCM2836_L1_IRQ_MAILBOX2     = 6,
    BCM2836_L1_IRQ_MAILBOX3     = 7,
    BCM2836_L1_IRQ_GPU          = 8,
    BCM2836_L1_IRQ_PMU          = 9,

    BCM2836_L1_IRQ_COUNT,

    /** IRQs that are managed per-core. */
    BCM2836_L1_IRQ_PERCPU       = (BCM2836_L1_IRQ_CNTPSIRQ |
                                   BCM2836_L1_IRQ_CNTPNSIRQ |
                                   BCM2836_L1_IRQ_CNTHPIRQ |
                                   BCM2836_L1_IRQ_CNTVIRQ |
                                   BCM2836_L1_IRQ_MAILBOX0 |
                                   BCM2836_L1_IRQ_MAILBOX1 |
                                   BCM2836_L1_IRQ_MAILBOX2 |
                                   BCM2836_L1_IRQ_MAILBOX3),
};

typedef struct bcm2836_l1_device {
    io_region_t io;
    irq_domain_t *domain;
} bcm2836_l1_device_t;

static inline uint32_t read_percpu_reg(bcm2836_l1_device_t *device, uint32_t reg, cpu_id_t cpu) {
    return io_read32(device->io, reg + (4 * cpu));
}

static inline void write_percpu_reg(bcm2836_l1_device_t *device, uint32_t reg, cpu_id_t cpu, uint32_t val) {
    io_write32(device->io, reg + (4 * cpu), val);
}

static inline uint32_t read_global_reg(bcm2836_l1_device_t *device, uint32_t reg) {
    return io_read32(device->io, reg);
}

static inline void write_global_reg(bcm2836_l1_device_t *device, uint32_t reg, uint32_t val) {
    io_write32(device->io, reg, val);
}

static bool bcm2836_l1_irq_pre_handle(irq_domain_t *domain, uint32_t num) {
    assert(false);
    return true;
}

static void bcm2836_l1_irq_post_handle(irq_domain_t *domain, uint32_t num, bool disable) {
    assert(false);
}

static irq_mode_t bcm2836_l1_irq_mode(irq_domain_t *domain, uint32_t num) {
    assert(false);
}

static void bcm2836_l1_irq_enable(irq_domain_t *domain, uint32_t num) {
    assert(false);
}

static void bcm2836_l1_irq_disable(irq_domain_t *domain, uint32_t num) {
    assert(false);
}

static irq_domain_ops_t bcm2836_l1_irq_ops = {
    .pre_handle  = bcm2836_l1_irq_pre_handle,
    .post_handle = bcm2836_l1_irq_post_handle,
    .mode        = bcm2836_l1_irq_mode,
    .enable      = bcm2836_l1_irq_enable,
    .disable     = bcm2836_l1_irq_disable,
};

static void bcm2836_l1_irq_handler(void *_device, frame_t *frame) {
    bcm2836_l1_device_t *device = _device;

    cpu_id_t cpu = curr_cpu->id;

    uint32_t pending = read_percpu_reg(device, BCM2836_L1_REG_PENDING0, cpu);

    kprintf(LOG_DEBUG, "received IRQ! 0x%x\n", pending);
    arm64_write_sysreg(cntv_ctl_el0, 0);
}

static status_t bcm2836_l1_irq_init_builtin(dt_device_t *dt) {
    if (dt->irq_parent != NULL) {
        kprintf(LOG_ERROR, "bcm2836_l1_irq: controller is expected to be the interrupt root\n");
        return STATUS_DEVICE_ERROR;
    }

    bcm2836_l1_device_t *device = kmalloc(sizeof(*device), MM_BOOT | MM_ZERO);
    dt->private = device;

    status_t ret = dt_reg_map(dt, 0, MM_BOOT, &device->io);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_ERROR, "bcm2836_l1_irq: failed to map registers: %d\n", ret);
        return ret;
    }

    /*
     * Default state after reset is to route all interrupts to the IRQ pin of
     * core 0. We don't need to change this.
     */

    device->domain = irq_domain_create(BCM2836_L1_IRQ_COUNT, &bcm2836_l1_irq_ops, device);
    dt_device_set_child_irq_domain(dt, device->domain);

    arm64_set_irq_handler(bcm2836_l1_irq_handler, device);

    uint64_t freq = arm64_read_sysreg(cntfrq_el0);
    uint64_t time = time_to_ticks(secs_to_nsecs(1), freq);

    write_percpu_reg(device, BCM2836_L1_REG_TIMER_INT_CONTROL0, 0, (1 << BCM2836_L1_IRQ_CNTVIRQ));

    local_irq_enable();
    while (true) {
        kprintf(LOG_DEBUG, "time 0x%lx\n", arm64_read_sysreg(cntv_ctl_el0));

        arm64_write_sysreg(cntv_tval_el0, time);
        arm64_write_sysreg(cntv_ctl_el0, (1<<0));

        spin(secs_to_nsecs(2));
    }

    return STATUS_SUCCESS;
}

static dt_match_t bcm2836_l1_irq_matches[] = {
    { .compatible = "brcm,bcm2836-l1-intc" },
};

static dt_driver_t bcm2836_l1_irq_driver = {
    .matches      = DT_MATCH_TABLE(bcm2836_l1_irq_matches),
    .builtin_type = BUILTIN_DT_DRIVER_IRQ,
    .init_builtin = bcm2836_l1_irq_init_builtin,
};

BUILTIN_DT_DRIVER(bcm2836_l1_irq_driver);
