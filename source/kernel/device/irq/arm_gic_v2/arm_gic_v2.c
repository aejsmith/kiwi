/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM GIC v2 IRQ controller driver.
 *
 * Documentation:
 *  - ARM Generic Interrupt Controller Architecture Specification
 *    https://developer.arm.com/documentation/ihi0048/b
 */

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
    GIC_REG_GICC_IAR                    = 0xc,
    GIC_REG_GICC_EOIR                   = 0x10,
};

/** CPU interface register bits. */
enum {
    GIC_GICC_CTLR_EnableGrp0            = (1<<0),
    GIC_GICC_CTLR_EnableGrp1            = (1<<1),

    GIC_GICC_IAR_InterruptID_SHIFT      = 0,
    GIC_GICC_IAR_InterruptID_MASK       = 0x3ff,

    GIC_GICC_EOIR_EOIINTID_SHIFT        = 0,
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

static void write_eoi(arm_gic_v2_device_t *device, uint32_t num) {
    // TODO: For SGI, need to write the CPU ID as well.
    write_cpu_reg(device, GIC_REG_GICC_EOIR, num << GIC_GICC_EOIR_EOIINTID_SHIFT);
}

static bool arm_gic_v2_irq_pre_handle(irq_domain_t *domain, uint32_t num, irq_mode_t mode) {
    arm_gic_v2_device_t *device = domain->private;

    if (mode == IRQ_MODE_EDGE)
        write_eoi(device, num);

    return true;
}

static void arm_gic_v2_irq_post_handle(irq_domain_t *domain, uint32_t num, irq_mode_t mode, bool disable) {
    arm_gic_v2_device_t *device = domain->private;

    if (mode == IRQ_MODE_LEVEL)
        write_eoi(device, num);
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

static uint32_t translate_irq(const uint32_t *prop) {
    switch (prop[0]) {
        case 0:
            /* SPI */
            return prop[1] + 32;
        case 1:
            /* PPI */
            return prop[1] + 16;
        default:
            kprintf(LOG_ERROR, "arm_gic_v2: invalid interrupt type %u\n", prop[0]);
            return UINT32_MAX;
    }
}

static void arm_gic_v2_dt_irq_configure(dt_device_t *controller, dt_device_t *child, uint32_t num) {
    uint32_t prop[3];
    bool success __unused = dt_irq_get_prop(child, num, prop);
    assert(success);

    uint32_t dest_num = translate_irq(prop);
    if (dest_num != UINT32_MAX) {
        irq_mode_t mode = dt_irq_mode(prop[2] & 0xff);
        status_t ret = irq_set_mode(controller->irq_controller.domain, dest_num, mode);
        if (ret != STATUS_SUCCESS) {
            kprintf(
                LOG_ERROR, "arm_gic_v2: failed to set mode %d for interrupt %u in device %s (dest_num: %u)\n",
                prop[2], num, child->name, dest_num);
        }
    }
}

static uint32_t arm_gic_v2_dt_irq_translate(dt_device_t *controller, dt_device_t *child, uint32_t num) {
    uint32_t prop[3];
    bool success = dt_irq_get_prop(child, num, prop);
    return (success) ? translate_irq(prop) : UINT32_MAX;
}

static dt_irq_ops_t arm_gic_v2_dt_irq_ops = {
    .configure = arm_gic_v2_dt_irq_configure,
    .translate = arm_gic_v2_dt_irq_translate,
};

static void arm_gic_v2_irq_handler(void *_device, frame_t *frame) {
    arm_gic_v2_device_t *device = _device;

    do {
        uint32_t iar = read_cpu_reg(device, GIC_REG_GICC_IAR);
        uint32_t num = (iar >> GIC_GICC_IAR_InterruptID_SHIFT) & GIC_GICC_IAR_InterruptID_MASK;

        if (num >= 1020)
            break;

        irq_handler(device->domain, num);
    } while (true);
}

static status_t arm_gic_v2_init_builtin(dt_device_t *dt) {
    status_t ret;

    if (dt->irq_parent != NULL) {
        // TODO: This isn't guaranteed, if it's not the root we'll need to
        // register our interrupts with the parent.
        kprintf(LOG_ERROR, "arm_gic_v2: %s: non-root interrupt controllers not currently supported\n", dt->name);
        return STATUS_DEVICE_ERROR;
    } else if (dt->irq_controller.num_cells != 3) {
        kprintf(LOG_ERROR, "arm_gic_v2: %s: unexpected number of interrupt cells\n", dt->name);
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
    dt_irq_init_controller(dt, device->domain, &arm_gic_v2_dt_irq_ops);

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
