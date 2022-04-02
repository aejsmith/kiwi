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
 * @brief               Kernel library initialisation function.
 */

#include <kernel/device.h>
#include <kernel/private/process.h>
#include <kernel/private/thread.h>
#include <kernel/object.h>
#include <kernel/system.h>

#include <elf.h>
#include <string.h>

#include "libkernel.h"

/** Saved environment parameters. */
bool libkernel_debug;               /**< Whether debug output is enabled. */
bool libkernel_dry_run;             /**< Whether this is a dry run. */

/** System page size. */
size_t page_size;

/** Kernel library main function.
 * @param args          Process argument block. */
void libkernel_init(process_args_t *args) {
    status_t ret;

    process_args = args;

    /* Get the system page size. */
    kern_system_info(SYSTEM_INFO_PAGE_SIZE, &page_size);

    /* Save the current process ID for the kern_process_id() wrapper. */
    _kern_process_id(PROCESS_SELF, &curr_process_id);

    /* If we're the first process, open handles to the kernel console. */
    if (curr_process_id == 1) {
        handle_t handle;
        kern_device_open("/virtual/kconsole", FILE_ACCESS_READ, 0, &handle);
        kern_handle_set_flags(handle, HANDLE_INHERITABLE);
        kern_device_open("/virtual/kconsole", FILE_ACCESS_WRITE, 0, &handle);
        kern_handle_set_flags(handle, HANDLE_INHERITABLE);
        kern_device_open("/virtual/kconsole", FILE_ACCESS_WRITE, 0, &handle);
        kern_handle_set_flags(handle, HANDLE_INHERITABLE);
    }

    /* Check if any of our options are set. */
    for (size_t i = 0; i < args->env_count; i++) {
        if (strncmp(args->env[i], "LIBKERNEL_DRY_RUN=", 18) == 0) {
            libkernel_dry_run = true;
        } else if (strncmp(args->env[i], "LIBKERNEL_DEBUG=", 16) == 0) {
            libkernel_debug = true;
        }
    }

    /* Initialise the runtime loader and load the program. */
    void (*entry)();
    ret = rtld_init((void **)&entry);
    if (ret != STATUS_SUCCESS || libkernel_dry_run)
        kern_process_exit(ret);

    /* Set up TLS for the current thread. */
    tls_tcb_t *tcb;
    ret = tls_alloc(&tcb);
    if (ret != STATUS_SUCCESS)
        kern_process_exit(ret);
    kern_thread_control(THREAD_SET_TLS_ADDR, tcb, NULL);

    /* Save the current thread ID in TLS for the kern_thread_id() wrapper. */
    _kern_thread_id(THREAD_SELF, &curr_thread_id);

    /* Let the kernel know where kern_thread_restore() is. */
    kern_process_control(PROCESS_SET_RESTORE, kern_thread_restore, NULL);

    /* Signal to the kernel that we've completed loading. */
    kern_process_control(PROCESS_LOADED, NULL, NULL);

    /* Run INIT functions for loaded images. */
    core_list_foreach(&loaded_images, iter) {
        rtld_image_t *image = core_list_entry(iter, rtld_image_t, header);
        void (*func)(void);

        if (image->dynamic[ELF_DT_INIT]) {
            func = (void (*)(void))(image->load_base + image->dynamic[ELF_DT_INIT]);
            dprintf("rtld: %s: calling INIT function %p...\n", image->name, func);
            func();
        }

        if (image->dynamic[ELF_DT_INIT_ARRAY]) {
            size_t count = image->dynamic[ELF_DT_INIT_ARRAYSZ] / sizeof(ptr_t);
            ptr_t *array = (ptr_t *)(image->load_base + image->dynamic[ELF_DT_INIT_ARRAY]);

            while (count--) {
                func = (void (*)(void))*array++;
                dprintf("rtld: %s: calling INIT_ARRAY function %p...\n", image->name, func);
                func();
            }
        }
    }

    /* Call the entry point for the program. */
    dprintf("libkernel: beginning program execution at %p...\n", entry);
    entry();
    dprintf("libkernel: program entry point returned\n");
    kern_process_exit(0);
}

/** Abort the process. */
void libkernel_abort(void) {
    exception_info_t info;
    info.code = EXCEPTION_ABORT;
    kern_thread_exception(&info);

    /* If this returns, then nuke any handlers and try again. */
    kern_process_set_exception_handler(EXCEPTION_ABORT, NULL);
    kern_thread_set_exception_handler(EXCEPTION_ABORT, NULL);
    kern_thread_exception(&info);

    kern_process_exit(-1);
}
