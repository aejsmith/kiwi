/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel console functions.
 */

#pragma once

#include <types.h>

struct kboot_tag_serial;
struct kboot_tag_video;

/** Kernel console output operations. */
typedef struct console_out_ops {
    /** Properly initialize the console after memory management setup. */
    void (*init)(void);

    /** Write a character to the console.
     * @param ch            Character to write. */
    void (*putc)(char ch);

    /** Write to the console without taking any locks (for fatal/KDB).
     * @param ch            Character to write. */
    void (*putc_unsafe)(char ch);
} console_out_ops_t;

/** Kernel console input operations structure. */
typedef struct console_in_ops {
    /**
     * Check for a character from the console. This function must be safe to
     * use from interrupt context, and should read directly from the device
     * rather than being driven by IRQs.
     *
     * @return              Character read, or 0 if none available.
     */
    uint16_t (*poll)(void);
} console_in_ops_t;

/** Special console key definitions. */
#define CONSOLE_KEY_UP      0x100
#define CONSOLE_KEY_DOWN    0x101
#define CONSOLE_KEY_LEFT    0x102
#define CONSOLE_KEY_RIGHT   0x103
#define CONSOLE_KEY_HOME    0x104
#define CONSOLE_KEY_END     0x105
#define CONSOLE_KEY_PGUP    0x106
#define CONSOLE_KEY_PGDN    0x107

/**
 * Kernel console structure.
 *
 * This structure defines a kernel console. We currently have two separate
 * consoles: the main console and the debug console. A console is made up of
 * separate input and output operations. The separation is necessary because
 * output and input may be handled in different places. They are probably the
 * same for the debug console (both handled by a serial driver), but on the
 * main console output may, for example, be handled by the framebuffer code,
 * while input is handled by the input driver.
 */
typedef struct console {
    const console_out_ops_t *out;   /**< Output operations. */
    const console_in_ops_t *in;     /**< Input operations. */
} console_t;

extern console_t main_console;
extern console_t debug_console;

extern void arch_console_early_init(struct kboot_tag_video *video, struct kboot_tag_serial *serial);

extern void console_early_init(void);
extern void console_init(void);

/** Framebuffer information structure. */
typedef struct fb_info {
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
    phys_ptr_t addr;                /**< Physical address of the framebuffer. */
} fb_info_t;

extern void fb_console_info(fb_info_t *info);
extern status_t fb_console_configure(const fb_info_t *info, uint32_t mmflag);
extern void fb_console_early_init(struct kboot_tag_video *video);
