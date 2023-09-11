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
 * @brief               Kernel initialization functions.
 */

#include <device/device.h>
#include <device/irq.h>

#include <io/fs.h>

#include <lib/random.h>
#include <lib/string.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/page_cache.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/ipc.h>
#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <security/token.h>

#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <object.h>
#include <smp.h>
#include <status.h>
#include <time.h>

KBOOT_IMAGE(KBOOT_IMAGE_LOG | KBOOT_IMAGE_SECTIONS);
KBOOT_BOOLEAN_OPTION("early_kdb", "Enter KDB during initialization", false);

extern initcall_t __initcall_start[], __initcall_end[];

static void init_thread(void *arg1, void *arg2);

/** Address of the KBoot tag list. */
static kboot_tag_t *kboot_tag_list __init_data;

/** Main entry point of the kernel.
 * @param magic         KBoot magic number.
 * @param tags          Tag list pointer. */
__init_text void kmain(uint32_t magic, kboot_tag_t *tags) {
    /* Save the tag list address. */
    kboot_tag_list = tags;

    /* Make the debugger available as soon as possible. */
    kdb_init();

    /* Bring up the console. */
    console_early_init();
    log_early_init();
    kprintf(LOG_NOTICE, "kernel: version %s booting...\n", kiwi_ver_string);

    /* Check the magic number. */
    if (magic != KBOOT_MAGIC) {
        kprintf(LOG_ERROR, "Not loaded by a KBoot-compliant loader\n");
        arch_cpu_halt();
    }

    /* Set up the CPU subsystem and initialize the boot CPU. */
    cpu_early_init();

    /* Initialize kernel memory management subsystems. */
    page_early_init();
    mmu_init();
    page_init();
    kmem_init();
    slab_init();
    malloc_init();

    /* We can now get to the ELF information passed by KBoot to enable us to do
     * symbol lookups. */
    module_early_init();

    /* Properly set up the console. */
    console_init();

    if (kboot_boolean_option("early_kdb"))
        kdb_enter(KDB_REASON_USER, NULL);

    device_early_init();
    cpu_init();
    arch_init();
    irq_init();
    time_init();
    random_init();
    smp_init();
    slab_late_init();
    object_init();
    token_init();
    ipc_init();
    process_init();
    thread_init();
    time_late_init();
    vm_init();
    page_cache_init();

    /* Create the second stage initialization thread. */
    status_t ret = thread_create("init", NULL, 0, init_thread, NULL, NULL, NULL);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create second-stage initialization thread");

    /* Finally begin executing other threads. */
    sched_enter();
}

/** Kernel entry point for a secondary CPU.
 * @param cpu           Pointer to CPU structure for the CPU. */
__init_text void kmain_secondary(cpu_t *cpu) {
    /* Indicate that we have reached the kernel. */
    smp_boot_status = SMP_BOOT_ALIVE;

    /* Initialize everything. */
    cpu_early_init_percpu(cpu);
    mmu_init_percpu();
    cpu_init_percpu();
    sched_init_percpu();
    time_init_percpu();

    /* Signal that we're up. */
    smp_boot_status = SMP_BOOT_BOOTED;

    /* Wait for remaining CPUs to be brought up. */
    while (smp_boot_status != SMP_BOOT_COMPLETE)
        arch_cpu_spin_hint();

    /* Begin scheduling threads. */
    sched_enter();
}

/** Second-stage intialization thread.
 * @param arg1          Unused.
 * @param arg2          Unused. */
static void init_thread(void *arg1, void *arg2) {
    status_t ret;

    /* Bring up all detected secondary CPUs. */
    smp_boot();

    kprintf(LOG_NOTICE, "cpu: detected %zu CPU(s):\n", cpu_count);
    list_foreach(&running_cpus, iter)
        cpu_dump(list_entry(iter, cpu_t, header));

    /* Bring up the filesystem manager and device manager. */
    device_init();
    fs_init();

    /* Call other initialization functions. */
    initcall_run(INITCALL_TYPE_DEVICE);
    initcall_run(INITCALL_TYPE_OTHER);

    update_boot_progress(10);

    /* Load modules loaded by KBoot. */
    module_init();

    update_boot_progress(20);

    /* Mount the root filesystem. */
    fs_mount_root();

    /* Reclaim memory taken up by initialization code/data. */
    mmu_late_init();
    page_late_init();
    kmem_late_init();

    update_boot_progress(30);

    /* Run the service manager. */
    const char *args[] = { "/system/services/service_manager", NULL };
    const char *env[]  = { NULL };
    ret = process_create(args, env, PROCESS_CREATE_CRITICAL, PRIORITY_CLASS_SYSTEM, NULL);
    if (ret != STATUS_SUCCESS)
        fatal("Could not start service manager (%d)", ret);
}

/** Run initcalls of a given type. */
__init_text void initcall_run(initcall_type_t type) {
    for (initcall_t *initcall = __initcall_start; initcall != __initcall_end; initcall++) {
        if (initcall->type == type)
            initcall->func();
    }
} 

/** Iterate over the KBoot tag list.
 * @param type          Type of tag to iterate.
 * @param current       Pointer to current tag, NULL to start from beginning.
 * @return              Virtual address of next tag, or NULL if no more tags
 *                      found. */
__init_text void *kboot_tag_iterate(uint32_t type, void *current) {
    kboot_tag_t *header = current;

    do {
        header = (header)
            ? (kboot_tag_t *)round_up((ptr_t)header + header->size, 8)
            : kboot_tag_list;
        if (header->type == KBOOT_TAG_NONE)
            header = NULL;
    } while (header && type && header->type != type);

    return header;
}

static __init_text void *lookup_option(const char *name, uint32_t type) {
    kboot_tag_foreach(KBOOT_TAG_OPTION, kboot_tag_option_t, tag) {
        const char *found = kboot_tag_data(tag, 0);

        if (strcmp(found, name) == 0) {
            if (tag->type != type)
                fatal("Kernel option `%s' has incorrect type", name);

            return kboot_tag_data(tag, round_up(tag->name_size, 8));
        }
    }

    /* If an option is specified in an image, there should be a tag passed to
     * the kernel for it. */
    fatal("Expected kernel option `%s' not found", name);
}

/** Get the value of a KBoot boolean option.
 * @param name          Name of the option.
 * @return              Value of the option. */
__init_text bool kboot_boolean_option(const char *name) {
    return *(bool *)lookup_option(name, KBOOT_OPTION_BOOLEAN);
}

/** Get the value of a KBoot integer option.
 * @param name          Name of the option.
 * @return              Value of the option. */
__init_text uint64_t kboot_integer_option(const char *name) {
    return *(uint64_t *)lookup_option(name, KBOOT_OPTION_INTEGER);
}

/** Get the value of a KBoot string option.
 * @param name          Name of the option.
 * @return              Pointer to the option string. */
__init_text const char *kboot_string_option(const char *name) {
    return (const char  *)lookup_option(name, KBOOT_OPTION_STRING);
}
