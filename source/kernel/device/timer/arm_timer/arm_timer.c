/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM generic timer driver.
 */

#include <arm64/cpu.h>

#include <device/bus/dt.h>
#include <device/irq.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

/** IRQ numbers in the DT binding. */
enum {
    ARM_TIMER_IRQ_SEC_PHYS,
    ARM_TIMER_IRQ_PHYS,
    ARM_TIMER_IRQ_VIRT,
    ARM_TIMER_IRQ_HYP_PHYS,
    ARM_TIMER_IRQ_HYP_VIRT,
};

enum {
    ARM_TIMER_CNTV_CTL_ENABLE = (1<<0),
};

typedef struct arm_timer_device {
    timer_device_t timer;
    irq_handler_t *irq;
    uint64_t freq;
} arm_timer_device_t;

static irq_status_t arm_timer_irq(void *_device) {
    arm64_write_sysreg(cntv_ctl_el0, 0);
    return (timer_tick()) ? IRQ_PREEMPT : IRQ_HANDLED;
}

static void arm_timer_prepare(timer_device_t *_device, nstime_t nsecs) {
    arm_timer_device_t *device = _device->private;

    uint64_t ticks = time_to_ticks(nsecs, device->freq);

    arm64_write_sysreg(cntv_tval_el0, ticks);
    arm64_write_sysreg(cntv_ctl_el0, ARM_TIMER_CNTV_CTL_ENABLE);
}

static timer_device_ops_t arm_timer_device_ops = {
    .type     = TIMER_DEVICE_ONESHOT,
    .prepare  = arm_timer_prepare,
};

static status_t arm_timer_init_builtin(dt_device_t *dt) {
    status_t ret;

    arm_timer_device_t *device = kmalloc(sizeof(*device), MM_BOOT);

    device->timer.name     = "ARM";
    device->timer.priority = 100;
    device->timer.ops      = &arm_timer_device_ops;
    device->timer.private  = device;

    device->freq = arm64_read_sysreg(cntfrq_el0);

    /* Just assume we're using the virtual IRQ for now... */
    ret = dt_irq_register(dt, ARM_TIMER_IRQ_VIRT, arm_timer_irq, NULL, device, &device->irq);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_ERROR, "arm_timer: %s: failed to register IRQ\n", dt->name);
        goto err;
    }

    time_set_timer_device(&device->timer);

    return STATUS_SUCCESS;

err:
    kfree(device);
    return ret;
}

static dt_match_t arm_timer_matches[] = {
    { .compatible = "arm,armv8-timer" },
    { .compatible = "arm,armv7-timer" },
};

static dt_driver_t arm_timer_driver = {
    .matches      = DT_MATCH_TABLE(arm_timer_matches),
    .builtin_type = BUILTIN_DT_DRIVER_TIME,
    .init_builtin = arm_timer_init_builtin,
};

BUILTIN_DT_DRIVER(arm_timer_driver);
