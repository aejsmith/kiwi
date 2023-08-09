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
 * @brief               ARM generic timer driver.
 */

#include <device/bus/dt.h>
#include <device/irq.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

static status_t arm_timer_init_builtin(dt_device_t *device) {
    kprintf(LOG_DEBUG, "hello from ARM timer\n");
    return STATUS_SUCCESS;
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
