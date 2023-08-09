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
 * @brief               Kernel framebuffer device.
 *
 * This provides basic access to the framebuffer used for the kernel console.
 * It does not provide any mode setting capability or any acceleration.
 *
 * Supported standard operations:
 *  - kern_vm_map():
 *    Map the framebuffer memory. Must have acquired exclusive access to the
 *    framebuffer with KFB_DEVICE_REQUEST_ACQUIRE.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** KFB device class name. */
#define KFB_DEVICE_CLASS_NAME           "kfb"

/** Framebuffer device requests. */
enum {
    /**
     * Requests details of the current mode.
     *
     * Output: Details of the current mode (kfb_mode_t)
     */
    KFB_DEVICE_REQUEST_MODE             = DEVICE_CLASS_REQUEST_START + 0,

    /**
     * Updates the boot progress bar.
     *
     * Input:  New completion percentage (uint32_t).
     */
    KFB_DEVICE_REQUEST_BOOT_PROGRESS    = DEVICE_CLASS_REQUEST_START + 1,

    /**
     * Acquires exclusive access to the framebuffer, required to be able to map
     * the framebuffer. Only one handle can have exclusive access at a time.
     * Access remains until the handle is closed and remaining mappings to it
     * have been unmapped.
     *
     * Errors: STATUS_IN_USE if another handle already has acquired exclusive
     *         access.
     */
    KFB_DEVICE_REQUEST_ACQUIRE          = DEVICE_CLASS_REQUEST_START + 2,
};

/** Framebuffer device events. */
enum {
    /**
     * Indicates that the framebuffer has been reconfigured. The user should
     * query the mode again and remap the framebuffer, and not continue to use
     * the previous mapping. Can only be delivered to a handle with exclusive
     * access, attempting to wait for this otherwise will return an error.
     */
    KFB_DEVICE_EVENT_RECONFIGURE        = DEVICE_CLASS_EVENT_START + 0,

    /**
     * Indicates that the user should redraw the framebuffer. This is fired
     * after the kernel has made use of the framebuffer itself, e.g. for KDB,
     * and has therefore overwritten its content. Can only be delivered to a
     * handle with exclusive access, attempting to wait for this otherwise will
     * return an error.
     */
    KFB_DEVICE_EVENT_REDRAW             = DEVICE_CLASS_EVENT_START + 1,
};

/** Framebuffer mode information structure. */
typedef struct kfb_mode {
    uint16_t width;                 /**< Width of the framebuffer. */
    uint16_t height;                /**< Height of the framebuffer. */
    uint8_t bytes_per_pixel;        /**< Bytes per pixel. */
    uint32_t pitch;                 /**< Number of bytes per line of the framebuffer. */
    uint8_t red_position;           /**< Red field position. */
    uint8_t red_size;               /**< Red field size. */
    uint8_t green_position;         /**< Green field position. */
    uint8_t green_size;             /**< Green field size. */
    uint8_t blue_position;          /**< Blue field position. */
    uint8_t blue_size;              /**< Blue field size. */
} kfb_mode_t;

__KERNEL_EXTERN_C_END
