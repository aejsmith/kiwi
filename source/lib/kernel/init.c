/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
        uint32_t modes[] = { FILE_ACCESS_READ, FILE_ACCESS_WRITE, FILE_ACCESS_WRITE };
        for (size_t i = 0; i < core_array_size(modes); i++) {
            handle_t handle;
            kern_device_open("/virtual/kconsole", modes[i], 0, &handle);

            /* Move into the standard IDs and make it inheritable. */
            kern_handle_duplicate(HANDLE_DUPLICATE_EXACT, handle, i, NULL);
            kern_handle_set_flags(i, HANDLE_INHERITABLE);
            kern_handle_close(handle);
        }
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
// TODO
#ifndef CONFIG_ARCH_ARM64
    tls_tcb_t *tcb;
    ret = tls_alloc(&tcb);
    if (ret != STATUS_SUCCESS)
        kern_process_exit(ret);
    kern_thread_control(THREAD_SET_TLS_ADDR, tcb, NULL);

    /* Save the current thread ID in TLS for the kern_thread_id() wrapper. */
    _kern_thread_id(THREAD_SELF, &curr_thread_id);
#endif

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
