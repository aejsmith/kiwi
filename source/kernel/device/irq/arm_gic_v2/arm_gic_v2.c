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
 * @brief               ARM GIC v2 IRQ controller driver.
 *
 * Documentation:
 *  - ARM Generic Interrupt Controller Architecture Specification
 *    https://developer.arm.com/documentation/ihi0048/b
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>

#include <device/bus/dt.h>
#include <device/irq.h>

#include <lib/utility.h>

#include <mm/malloc.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** Distributor register definitions. */
enum {
    GIC_REG_GICD_CTLR                   = 0x0,
    GIC_REG_GICD_TYPER                  = 0x4,
    GIC_REG_GICD_ISENABLEn              = 0x100,
    GIC_REG_GICD_ICENABLEn              = 0x180,
    GIC_REG_GICD_ICFGRn                 = 0xc00,
};

/** Distributor register bits. */
enum {
    GIC_GICD_CTLR_EnableGrp0            = (1<<0),
    GIC_GICD_CTLR_EnableGrp1            = (1<<1),

    GIC_GICD_TYPER_ITLinesNumber_SHIFT  = 0,
    GIC_GICD_TYPER_ITLinesNumber_MASK   = 0xf,
};

/** CPU interface register definitions. */
enum {
    GIC_REG_GICC_CTLR                   = 0x0,
    GIC_REG_GICC_PMR                    = 0x4,
    GIC_REG_GICC_BPR                    = 0x8,
};

/** CPU interface register bits. */
enum {
    GIC_GICC_CTLR_EnableGrp0            = (1<<0),
    GIC_GICC_CTLR_EnableGrp1            = (1<<1),
};

typedef struct arm_gic_v2_device {
    dt_device_t *dt;
    spinlock_t lock;
    io_region_t distrib_io;
    io_region_t cpu_io;
    irq_domain_t *domain;
} arm_gic_v2_device_t;

static inline uint32_t read_distrib_reg(arm_gic_v2_device_t *device, uint32_t reg) {
    return io_read32(device->distrib_io, reg);
}

static inline void write_distrib_reg(arm_gic_v2_device_t *device, uint32_t reg, uint32_t val) {
    io_write32(device->distrib_io, reg, val);
}

static inline uint32_t read_cpu_reg(arm_gic_v2_device_t *device, uint32_t reg) {
    return io_read32(device->cpu_io, reg);
}

static inline void write_cpu_reg(arm_gic_v2_device_t *device, uint32_t reg, uint32_t val) {
    io_write32(device->cpu_io, reg, val);
}

static bool arm_gic_v2_irq_pre_handle(irq_domain_t *domain, uint32_t num) {
    assert(false);
    return true;
}

static void arm_gic_v2_irq_post_handle(irq_domain_t *domain, uint32_t num, bool disable) {
    assert(false);
}

static irq_mode_t arm_gic_v2_irq_mode(irq_domain_t *domain, uint32_t num) {
    arm_gic_v2_device_t *device = domain->private;

    if (num < 16) {
        /* SGIs are always edge-triggered. */
        return IRQ_MODE_EDGE;
    }

    spinlock_lock(&device->lock);

    uint32_t reg    = GIC_REG_GICD_ICFGRn + (num / 16) * 4;
    uint32_t bit    = 1 << (((num % 16) * 2) + 1);
    uint32_t val    = read_distrib_reg(device, reg);
    irq_mode_t mode = (val & bit) ? IRQ_MODE_EDGE : IRQ_MODE_LEVEL;

    spinlock_unlock(&device->lock);

    return mode;
}

static status_t arm_gic_v2_irq_set_mode(irq_domain_t *domain, uint32_t num, irq_mode_t mode) {
    arm_gic_v2_device_t *device = domain->private;

    if (num < 16) {
        /* SGIs are always edge-triggered. */
        if (mode != IRQ_MODE_EDGE)
            return STATUS_NOT_SUPPORTED;
    }

    spinlock_lock(&device->lock);

    uint32_t reg = GIC_REG_GICD_ICFGRn + ((num / 16) * 4);
    uint32_t bit = 1 << (((num % 16) * 2) + 1);
    uint32_t old = read_distrib_reg(device, reg);
    uint32_t new = (mode == IRQ_MODE_EDGE) ? old & ~bit : old | bit;

    write_distrib_reg(device, reg, new);

    status_t ret = STATUS_SUCCESS;
    if (read_distrib_reg(device, reg) != new) {
        /* For PPIs it is implementation-defined whether they are configurable
         * so this may fail. */
        kprintf(LOG_DEBUG, "arm_gic_v2: %s: failed to change mode for IRQ %u\n", device->dt->name, num);
        ret = STATUS_DEVICE_ERROR;
    }

    spinlock_unlock(&device->lock);
    return ret;
}

static void arm_gic_v2_irq_enable(irq_domain_t *domain, uint32_t num) {
    arm_gic_v2_device_t *device = domain->private;

    spinlock_lock(&device->lock);

    uint32_t reg = GIC_REG_GICD_ISENABLEn + ((num / 32) * 4);
    uint32_t bit = 1 << (num % 32);

    write_distrib_reg(device, reg, bit);

    // TODO: This is banked, needs to be done for all CPUs when registering
    // but locally while handling.

    spinlock_unlock(&device->lock);
}

static void arm_gic_v2_irq_disable(irq_domain_t *domain, uint32_t num) {
    arm_gic_v2_device_t *device = domain->private;

    spinlock_lock(&device->lock);

    uint32_t reg = GIC_REG_GICD_ICENABLEn + ((num / 32) * 4);
    uint32_t bit = 1 << (num % 32);

    write_distrib_reg(device, reg, bit);

    // TODO: This is banked, needs to be done for all CPUs when unregistering
    // but locally while handling.

    spinlock_unlock(&device->lock);
}

static irq_domain_ops_t arm_gic_v2_irq_ops = {
    .pre_handle  = arm_gic_v2_irq_pre_handle,
    .post_handle = arm_gic_v2_irq_post_handle,
    .mode        = arm_gic_v2_irq_mode,
    .set_mode    = arm_gic_v2_irq_set_mode,
    .enable      = arm_gic_v2_irq_enable,
    .disable     = arm_gic_v2_irq_disable,
};

static void arm_gic_v2_irq_handler(void *_device, frame_t *frame) {
    arm_gic_v2_device_t *device = _device;

    (void)device;
    assert(false);
}

static status_t arm_gic_v2_init_builtin(dt_device_t *dt) {
    status_t ret;

    if (dt->irq_parent != NULL) {
        // TODO: This isn't guaranteed, if it's not the root we'll need to
        // register our interrupts with the parent.
        kprintf(LOG_ERROR, "arm_gic_v2: non-root interrupt controllers not currently supported\n");
        return STATUS_DEVICE_ERROR;
    }

    arm_gic_v2_device_t *device = kmalloc(sizeof(*device), MM_BOOT | MM_ZERO);
    dt->private = device;

    spinlock_init(&device->lock, "arm_gic_v2_device_lock");

    device->dt = dt;

    ret = dt_reg_map(dt, 0, MM_BOOT, &device->distrib_io);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_ERROR, "arm_gic_v2: %s: failed to map distributor registers: %d\n", dt->name, ret);
        return ret;
    }

    ret = dt_reg_map(dt, 1, MM_BOOT, &device->cpu_io);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_ERROR, "arm_gic_v2: %s: failed to map CPU registers: %d\n", dt->name, ret);
        return ret;
    }

    uint32_t typer     = read_distrib_reg(device, GIC_REG_GICD_TYPER);
    uint32_t irq_count = (typer >> GIC_GICD_TYPER_ITLinesNumber_SHIFT) & GIC_GICD_TYPER_ITLinesNumber_MASK;
    irq_count          = min((irq_count + 1) * 32, 1020u);
    kprintf(LOG_NOTICE, "arm_gic_v2: %s: %u IRQ lines\n", dt->name, irq_count);

    device->domain = irq_domain_create(irq_count, &arm_gic_v2_irq_ops, device);
    dt_device_set_child_irq_domain(dt, device->domain);

    if (dt->irq_parent) {
        // TODO
    } else {
        arm64_set_irq_handler(arm_gic_v2_irq_handler, device);
    }

    /* Disable GICD/GICC before configuring. */
    write_distrib_reg(device, GIC_REG_GICD_CTLR, 0);
    write_cpu_reg(device, GIC_REG_GICC_CTLR, 0);
    
    /* Set priority mask to allow all interrupts. */
    write_cpu_reg(device, GIC_REG_GICC_PMR, 0xff);

    /* Set binary point register to disable preemption. */
    write_cpu_reg(device, GIC_REG_GICC_BPR, 7);

    /* Re-enable GICD/GICC. */
    write_cpu_reg(device, GIC_REG_GICC_CTLR, GIC_GICC_CTLR_EnableGrp0 | GIC_GICC_CTLR_EnableGrp1);
    write_distrib_reg(device, GIC_REG_GICD_CTLR, GIC_GICD_CTLR_EnableGrp0 | GIC_GICD_CTLR_EnableGrp1);

    uint64_t freq = arm64_read_sysreg(cntfrq_el0);
    uint64_t time = time_to_ticks(secs_to_nsecs(1), freq);

    arm_gic_v2_irq_set_mode(device->domain, 16 + 0xd, IRQ_MODE_LEVEL);
    arm_gic_v2_irq_enable(device->domain, 16 + 0xd);
    arm_gic_v2_irq_set_mode(device->domain, 16 + 0xe, IRQ_MODE_LEVEL);
    arm_gic_v2_irq_enable(device->domain, 16 + 0xe);
    arm_gic_v2_irq_set_mode(device->domain, 16 + 0xb, IRQ_MODE_LEVEL);
    arm_gic_v2_irq_enable(device->domain, 16 + 0xb);
    arm_gic_v2_irq_set_mode(device->domain, 16 + 0xa, IRQ_MODE_LEVEL);
    arm_gic_v2_irq_enable(device->domain, 16 + 0xa);
//    write_percpu_reg(device, arm_gic_v2_REG_TIMER_INT_CONTROL0, 0, (1 << arm_gic_v2_IRQ_CNTVIRQ));

    local_irq_enable();
    while (true) {
        kprintf(LOG_DEBUG, "time 0x%lx\n", arm64_read_sysreg(cntv_ctl_el0));

        arm64_write_sysreg(cntv_tval_el0, time);
        arm64_write_sysreg(cntv_ctl_el0, (1<<0));

        spin(secs_to_nsecs(2));
    }

    return STATUS_SUCCESS;
}

static dt_match_t arm_gic_v2_matches[] = {
    { .compatible = "arm,cortex-a15-gic" },
};

static dt_driver_t arm_gic_v2_driver = {
    .matches      = DT_MATCH_TABLE(arm_gic_v2_matches),
    .builtin_type = BUILTIN_DT_DRIVER_IRQ,
    .init_builtin = arm_gic_v2_init_builtin,
};

BUILTIN_DT_DRIVER(arm_gic_v2_driver);
