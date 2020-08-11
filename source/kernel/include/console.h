/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Kernel console functions.
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <lib/list.h>

struct kboot_tag_video;

/**
 * Kernel console output operations.
 *
 * This structure defines operations used for kernel console output. Console
 * output should be possible under any circumstance and therefore these
 * functions must be usable in interrupt context. While for normal kernel
 * output they are called with the kernel log lock held, they can also be
 * called without, so they should perform locking themselves.
 */
typedef struct console_out_ops {
    /** Properly initialize the console after memory management setup.
     * @param video         KBoot video tag. */
    void (*init)(struct kboot_tag_video *video);

    /** Write a character to the console.
     * @param ch            Character to write. */
    void (*putc)(char ch);
} console_out_ops_t;

/** Kernel console input operations structure. */
typedef struct console_in_ops {
    /** Check for a character from the console.
     * @note                This function must be safe to use from interrupt
     *                      context.
     * @return              Character read, or 0 if none available. */
    uint16_t (*poll)(void);

    /** Read a character from the console, blocking until it can do so.
     * @param _ch           Where to store character read.
     * @return              Status code describing the result of the operation. */
    status_t (*getc)(uint16_t *_ch);
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
    console_out_ops_t *out;         /**< Output operations. */
    console_in_ops_t *in;           /**< Input operations. */
} console_t;

extern console_t main_console;
extern console_t debug_console;

extern void platform_console_early_init(struct kboot_tag_video *video);

extern void console_early_init(void);
extern void console_init(void);

/** Framebuffer information structure. */
typedef struct fb_info {
    uint16_t width;                 /**< Width of the framebuffer. */
    uint16_t height;                /**< Height of the framebuffer. */
    uint8_t depth;                  /**< Colour depth of the framebuffer (bits per pixel). */
    uint8_t bytes_per_pixel;        /**< Bytes per pixel. */
    uint8_t red_position;           /**< Red field position. */
    uint8_t red_size;               /**< Red field size. */
    uint8_t green_position;         /**< Green field position. */
    uint8_t green_size;             /**< Green field size. */
    uint8_t blue_position;          /**< Blue field position. */
    uint8_t blue_size;              /**< Blue field size. */
    phys_ptr_t addr;                /**< Physical address of the framebuffer. */
} fb_info_t;

extern void fb_console_info(fb_info_t *info);
extern status_t fb_console_configure(const fb_info_t *info, unsigned mmflag);
extern void fb_console_acquire(void);
extern void fb_console_release(void);
extern void fb_console_early_init(struct kboot_tag_video *video);

#endif /* __CONSOLE_H */
