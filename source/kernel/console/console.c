/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel console functions.
 */

#include <device/console/serial.h>

#include <device/device.h>

#include <io/request.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <console.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

KBOOT_VIDEO(KBOOT_VIDEO_LFB, 0, 0, 0);

/** Main console. */
console_t main_console;

/** Debug console. */
console_t debug_console;

/** Initialize the debug console. */
__init_text void console_early_init(void) {
    kboot_tag_video_t *video   = kboot_tag_iterate(KBOOT_TAG_VIDEO, NULL);
    kboot_tag_serial_t *serial = kboot_tag_iterate(KBOOT_TAG_SERIAL, NULL);

    arch_console_early_init(video, serial);

    /* Try to set up a serial port if the architecture didn't. */
    if (!debug_console.out && serial)
        serial_console_early_init(serial);

    /* Set up a framebuffer console if the architecture didn't. */
    if (!main_console.out && video && video->type == KBOOT_VIDEO_LFB)
        fb_console_early_init(video);
}

/** Initialize the primary console. */
__init_text void console_init(void) {
    if (debug_console.out && debug_console.out->init)
        debug_console.out->init();

    if (main_console.out && main_console.out->init)
        main_console.out->init();
}

/*
 * Kernel console device functions.
 */

/** Perform I/O on the kernel console device. */
static status_t kconsole_device_io(device_t *device, file_handle_t *handle, io_request_t *request) {
    status_t ret;

    if (request->op != IO_OP_WRITE)
        return STATUS_NOT_SUPPORTED;

    char *buf = kmalloc(request->total, MM_USER);
    if (!buf)
        return STATUS_NO_MEMORY;

    ret = io_request_copy(request, buf, request->total, true);
    if (ret != STATUS_SUCCESS)
        goto out;

    for (size_t i = 0; i < request->total; i++) {
        if (main_console.out)
            main_console.out->putc(buf[i]);
        if (debug_console.out)
            debug_console.out->putc(buf[i]);
    }

out:
    kfree(buf);
    return ret;
}

/** Kernel console device operations structure. */
static const device_ops_t kconsole_device_ops = {
    .type = FILE_TYPE_CHAR,
    .io   = kconsole_device_io,
};

/** Register the kernel console device. */
static __init_text void kconsole_device_init(void) {
    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = "kconsole" } },
    };

    device_t *device;
    status_t ret = device_create(
        "kconsole", device_virtual_dir, &kconsole_device_ops, NULL, attrs,
        array_size(attrs), &device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to register kernel console device (%d)", ret);

    device_publish(device);
}

INITCALL(kconsole_device_init);
