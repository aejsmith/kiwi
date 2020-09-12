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
 * @brief               Process management functions.
 *
 * TODO:
 *  - Finer grained locking on processes? For example, we could probably have a
 *    separate lock for security information.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <lib/id_allocator.h>
#include <lib/string.h>

#include <io/fs.h>

#include <ipc/ipc.h>

#include <kernel/private/process.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <security/security.h>

#include <sync/futex.h>
#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <elf.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>
#include <time.h>

/** Define to enable debug output on thread creation/deletion. */
//#define DEBUG_PROCESS

#ifdef DEBUG_PROCESS
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Expected path to libkernel. */
#define LIBKERNEL_PATH      "/system/lib/libkernel.so"

/** Structure containing process loading state. */
typedef struct process_load {
    char *path;                     /**< Path to program. */
    char **args;                    /**< Argument array. */
    size_t arg_count;               /**< Argument count. */
    char **env;                     /**< Environment array. */
    size_t env_count;               /**< Environment variable count. */
    handle_t (*map)[2];             /**< Handle mapping array. */
    ssize_t map_count;              /**< Size of mapping array. */

    vm_aspace_t *aspace;            /**< Address space for the process. */
    token_t *token;                 /**< Token for the process. */
    ipc_port_t *root_port;          /**< Root port for the process. */

    elf_image_t *image;             /**< ELF loader data. */
    ptr_t arg_block;                /**< Address of argument block mapping. */
    ptr_t stack;                    /**< Address of stack mapping. */

    semaphore_t sem;                /**< Semaphore to wait for completion on. */
    status_t status;                /**< Status code to return from the call. */
} process_load_t;

/** Tree of all processes. */
static AVL_TREE_DEFINE(process_tree);
static RWLOCK_DEFINE(process_tree_lock);

/** Process ID allocator. */
static id_allocator_t process_id_allocator;

/** Cache for process structures. */
static slab_cache_t *process_cache;

/** Handle to the kernel library. */
static object_handle_t *kernel_library;

/** Process containing all kernel-mode threads. */
process_t *kernel_proc;

/** Constructor for process objects.
 * @param obj           Pointer to object.
 * @param data          Ignored. */
static void process_ctor(void *obj, void *data) {
    process_t *process = (process_t *)obj;

    mutex_init(&process->lock, "process_lock", 0);
    refcount_set(&process->running, 0);
    list_init(&process->threads);
    avl_tree_init(&process->futexes);
    list_init(&process->images);
    notifier_init(&process->death_notifier, process);
}

/** Allocate and initialize a new process structure.
 * @param name          Name to give the process.
 * @param id            If negative, allocate an ID, else the exact ID to use.
 * @param parent        Parent to inherit information from.
 * @param priority      Priority class to give the process.
 * @param aspace        Address space for the process.
 * @param token         Security token for the process (should be referenced).
 * @param root_port     Root port for the process (should be referenced).
 * @param _process      Where to store pointer to created process structure.
 *                      Reference count will be set to 1.
 * @return              STATUS_SUCCESS if successful, STATUS_PROCESS_LIMIT if
 *                      unable to allocate an ID. */
static status_t process_alloc(
    const char *name, process_id_t id, process_t *parent, int priority,
    vm_aspace_t *aspace, token_t *token, ipc_port_t *root_port,
    process_t **_process)
{
    process_t *process;

    if (id < 0) {
        id = id_allocator_alloc(&process_id_allocator);
        if (id < 0)
            return STATUS_PROCESS_LIMIT;
    }

    process = slab_cache_alloc(process_cache, MM_KERNEL);
    memset(process->exceptions, 0, sizeof(process->exceptions));
    refcount_set(&process->count, 1);
    io_process_init(process, parent);
    object_process_init(process);
    process->flags = 0;
    process->priority = priority;
    process->token = token;
    process->aspace = aspace;
    process->thread_restore = 0;
    process->root_port = root_port;
    process->state = PROCESS_CREATED;
    process->id = id;
    process->name = kstrdup(name, MM_KERNEL);
    process->status = 0;
    process->reason = EXIT_REASON_NORMAL;
    process->load = NULL;

    /* Add to the process tree. */
    rwlock_write_lock(&process_tree_lock);
    avl_tree_insert(&process_tree, process->id, &process->tree_link);
    rwlock_unlock(&process_tree_lock);

    dprintf(
        "process: created process %" PRId32 " (%s) (process: %p, parent: %p)\n",
        process->id, process->name, process, parent);

    *_process = process;
    return STATUS_SUCCESS;
}

/** Free a process' resources after it has died.
 * @param process       Process to clean up. */
static void process_cleanup(process_t *process) {
    elf_process_cleanup(process);
    futex_process_cleanup(process);

    if (process->aspace) {
        vm_aspace_destroy(process->aspace);
        process->aspace = NULL;
    }

    if (process->root_port)
        ipc_port_release(process->root_port);

    io_process_cleanup(process);
    object_process_cleanup(process);

    notifier_clear(&process->death_notifier);
}

/**
 * Increase the reference count of a process.
 *
 * Increases the reference count of a process. This should be done when you
 * want to ensure that the process will not freed: it will only be freed once
 * the count reaches 0.
 *
 * @param process       Process to retain.
 */
void process_retain(process_t *process) {
    refcount_inc(&process->count);
}

/**
 * Decrease the reference count of a process.
 *
 * Decreases the reference count of a process. This should be called once you
 * no longer require a process object (that was returned from process_create()
 * or process_lookup(), or that you previously called thread_retain() on). Once
 * the reference count reaches 0, the process will be destroyed.
 *
 * @param process       Process to release.
 */
void process_release(process_t *process) {
    if (refcount_dec(&process->count) > 0)
        return;

    assert(process->state != PROCESS_RUNNING);
    assert(refcount_get(&process->running) == 0);

    /* If no threads in the process have been run we still have to clean it up
     * as it will not have gone through thread_exit(). */
    if (process->state == PROCESS_CREATED)
        process_cleanup(process);

    rwlock_write_lock(&process_tree_lock);
    avl_tree_remove(&process_tree, &process->tree_link);
    rwlock_unlock(&process_tree_lock);

    token_release(process->token);
    id_allocator_free(&process_id_allocator, process->id);

    dprintf(
        "process: destroyed process %" PRId32 " (%s) (process: %p, status: %d)\n",
        process->id, process->name, process, process->status);

    kfree(process->name);
    slab_cache_free(process_cache, process);
}

/** Attach a thread to a process.
 * @param process       Process to attach to.
 * @param thread        Thread to attach. */
void process_attach_thread(process_t *process, thread_t *thread) {
    thread->owner = process;

    mutex_lock(&process->lock);

    assert(process->state != PROCESS_DEAD);
    list_append(&process->threads, &thread->owner_link);
    refcount_inc(&process->count);

    mutex_unlock(&process->lock);
}

/** Increment the running count of a thread's owner.
 * @param thread        Thread that has started running. */
void process_thread_started(thread_t *thread) {
    assert(thread->owner->state != PROCESS_DEAD);

    if (refcount_inc(&thread->owner->running) == 1)
        thread->owner->state = PROCESS_RUNNING;
}

/** Decrement the running count of a thread's owner.
 * @param thread        Thread that has exited. */
void process_thread_exited(thread_t *thread) {
    process_t *process = thread->owner;

    assert(process->state == PROCESS_RUNNING);

    if (refcount_dec(&process->running) == 0) {
        /* All threads have terminated, move the process to the dead state and
         * clean up its resources. */
        process->state = PROCESS_DEAD;

        if (process->flags & PROCESS_CRITICAL && !shutdown_in_progress)
            fatal("Critical process %" PRId32 " (%s) terminated", process->id, process->name);

        /* Don't bother running callbacks during shutdown. */
        if (!shutdown_in_progress)
            notifier_run(&process->death_notifier, NULL, true);

        process_cleanup(process);

        /* If the load pointer is not NULL, a process_create() call is waiting.
         * Make it return with the exit code. */
        if (process->load) {
            process->load->status = process->status;
            semaphore_up(&process->load->sem, 1);
            process->load = NULL;
        }
    }
}

/** Detach a thread from its owner.
 * @param thread        Thread to detach. */
void process_detach_thread(thread_t *thread) {
    process_t *process = thread->owner;

    mutex_lock(&process->lock);
    list_remove(&thread->owner_link);
    mutex_unlock(&process->lock);

    thread->owner = NULL;

    process_release(process);
}

/** Version of process_access() with process lock already held.
 * @param process       Process to check.
 * @return              Whether current thread has privileged access. */
bool process_access_unsafe(process_t *process) {
    if (process != curr_proc) {
        if (!security_check_priv(PRIV_PROCESS_ADMIN)) {
            if (security_current_uid() != process->token->ctx.uid)
                return false;
        }
    }

    return true;
}

/**
 * Check if the current thread has privileged access to a process.
 *
 * Checks if the current thread has privileged access to a process, i.e. either
 * it has the PRIV_PROCESS_ADMIN privilege, or the user IDs of the thread and
 * the process match.
 *
 * @param process       Process to check.
 *
 * @return              Whether current thread has privileged access.
 */
bool process_access(process_t *process) {
    bool ret = true;

    if (process != curr_proc) {
        mutex_lock(&process->lock);
        ret = process_access_unsafe(process);
        mutex_unlock(&process->lock);
    }

    return ret;
}

/** Terminate the calling process and all of its threads. */
void process_exit(void) {
    thread_t *thread;

    mutex_lock(&curr_proc->lock);

    list_foreach_safe(&curr_proc->threads, iter) {
        thread = list_entry(iter, thread_t, owner_link);

        if (thread != curr_thread)
            thread_kill(thread);
    }

    mutex_unlock(&curr_proc->lock);

    thread_exit();
}

/**
 * Look up a process without taking the tree lock.
 *
 * Looks up a process by its ID, without taking the tree lock. The returned
 * process will not have an extra reference on it.
 *
 * This function should only be used within KDB. Use process_lookup() outside
 * of KDB.
 *
 * @param id            ID of the process to find.
 *
 * @return              Pointer to process found, or NULL if not found.
 */
process_t *process_lookup_unsafe(process_id_t id) {
    return avl_tree_lookup(&process_tree, id, process_t, tree_link);
}

/**
 * Look up a process.
 *
 * Looks up a process by its ID. If the process is found, it will be returned
 * with a reference added to it. Once it is no longer needed, process_release()
 * should be called on it.
 *
 * @param id            ID of the process to find.
 *
 * @return              Pointer to process found, or NULL if not found.
 */
process_t *process_lookup(process_id_t id) {
    process_t *process;

    rwlock_read_lock(&process_tree_lock);

    process = process_lookup_unsafe(id);
    if (process)
        process_retain(process);

    rwlock_unlock(&process_tree_lock);
    return process;
}

/**
 * Executable loader.
 */

/** Set up a new address space for a process.
 * @param load          Loading information structure.
 * @param parent        Parent process (can be NULL).
 * @return              Status code describing result of the operation. */
static status_t process_load(process_load_t *load, process_t *parent) {
    object_handle_t *handle;
    size_t size;
    status_t ret;

    semaphore_init(&load->sem, "process_load_sem", 0);
    load->aspace = vm_aspace_create();

    /* Reserve space for the binary being loaded in the address space. The
     * actual loading of it is done by the kernel library's loader, however we
     * must reserve space to ensure that the mappings we create below for the
     * arguments/stack don't end up placed where the binary wants to be. */
    ret = fs_open(load->path, FILE_ACCESS_READ | FILE_ACCESS_EXECUTE, 0, 0, &handle);
    if (ret != STATUS_SUCCESS)
        goto fail;

    ret = elf_binary_reserve(handle, load->aspace);
    object_handle_release(handle);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* If the kernel library has not been opened, open it now. We keep a handle
     * to it open all the time so that if it gets replaced by a new version,
     * the new version won't actually be used until the system is rebooted.
     * This avoids problems if a new kernel is not ABI-compatible with the
     * previous kernel. */
    if (!kernel_library) {
        ret = fs_open(
            LIBKERNEL_PATH, FILE_ACCESS_READ | FILE_ACCESS_EXECUTE, 0, 0,
            &kernel_library);
        if (ret != STATUS_SUCCESS)
            fatal("Could not open kernel library (%d)", ret);
    }

    /* Map the kernel library. */
    ret = elf_binary_load(kernel_library, LIBKERNEL_PATH, load->aspace, &load->image);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* Determine the size of the argument block. Each argument/environment
     * entry requires the length of the string plus another pointer for the
     * array entry, and 2 more pointers for the NULL terminators. */
    size = sizeof(process_args_t) + strlen(load->path) + (sizeof(char *) * 2);
    for (
        load->arg_count = 0;
        load->args[load->arg_count];
        size += (strlen(load->args[load->arg_count++]) + sizeof(char *)));
    for (
        load->env_count = 0;
        load->env[load->env_count];
        size += (strlen(load->env[load->env_count++]) + sizeof(char *)));
    size = round_up(size, PAGE_SIZE);

    /* Create a mapping for it. */
    ret = vm_map(
        load->aspace, &load->arg_block, size, 0, VM_ADDRESS_ANY,
        VM_ACCESS_READ | VM_ACCESS_WRITE, VM_MAP_PRIVATE, NULL, 0,
        "process_args");
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Create a stack mapping. */
    ret = vm_map(
        load->aspace, &load->stack, USTACK_SIZE, USTACK_SIZE, VM_ADDRESS_ANY,
        VM_ACCESS_READ | VM_ACCESS_WRITE, VM_MAP_PRIVATE | VM_MAP_STACK,
        NULL, 0, "main_stack");
    if (ret != STATUS_SUCCESS)
        goto fail;

    return STATUS_SUCCESS;

fail:
    vm_aspace_destroy(load->aspace);
    load->aspace = NULL;
    return ret;
}

/** Copy the data contained in a string array to the argument block.
 * @param dest          Array to store addresses copied to in.
 * @param source        Array to copy data of.
 * @param count         Number of array entries.
 * @param base          Pointer to address to copy to, updated after copying. */
static void copy_argument_strings(char **dest, char **source, size_t count, ptr_t *base) {
    size_t i, len;

    for (i = 0; i < count; i++) {
        dest[i] = (char *)(*base);
        len = strlen(source[i]) + 1;
        memcpy(dest[i], source[i], len);
        *base += len;
    }

    dest[count] = NULL;
}

/** Entry point for a new process.
 * @param arg1          Pointer to creation information structure.
 * @param arg2          Unused. */
static void process_entry_trampoline(void *arg1, void *arg2) {
    process_load_t *load = arg1;
    ptr_t addr, stack, entry;
    process_args_t *uargs;
    frame_t frame;

    /* Copy stack details to the thread so that it'll get unmapped if this
     * thread exits. */
    curr_thread->ustack = load->stack;
    curr_thread->ustack_size = USTACK_SIZE;

    /* Fill out the argument block. It's safe for us to write directly to it in
     * this function, as we created this mapping and since we're the only thread
     * in the process so far, nothing else could have unmapped it. */
    addr = load->arg_block;
    uargs = (process_args_t *)addr;
    addr += sizeof(process_args_t);
    uargs->path = (char *)addr;
    addr += strlen(load->path) + 1;
    uargs->args = (char **)addr;
    addr += (load->arg_count + 1) * sizeof(char *);
    uargs->env = (char **)addr;
    addr += (load->env_count + 1) * sizeof(char *);
    uargs->arg_count = load->arg_count;
    uargs->env_count = load->env_count;
    uargs->load_base = (void *)load->image->load_base;

    /* Copy path string, arguments and environment variables. */
    strcpy(uargs->path, load->path);
    copy_argument_strings(uargs->args, load->args, load->arg_count, &addr);
    copy_argument_strings(uargs->env, load->env, load->env_count, &addr);

    /* Get the stack pointer and save the argument block pointer. */
    stack = load->stack + USTACK_SIZE;
    addr = load->arg_block;

    /* Get the ELF loader to clear BSS and get the entry pointer. */
    entry = elf_binary_finish(load->image);

    /* If there the information structure pointer is NULL, the process is being
     * created via kern_process_exec() and we don't need to wait for the loader
     * to complete. */
    if (!curr_proc->load)
        semaphore_up(&load->sem, 1);

    dprintf(
        "process: entering user mode in new process (entry: %p, stack: %p, args: %p)\n",
        entry, stack, addr);

    arch_thread_user_setup(&frame, entry, stack, addr);
    arch_thread_user_enter(&frame);
}

/**
 * Execute a new process.
 *
 * Creates a new process and runs a program within it. The path to the program
 * should be the first entry in the argument array. The new process will
 * inherit no information from the calling process. This process will be
 * created with the system token.
 *
 * @param args          Arguments to pass to process (NULL-terminated array).
 * @param env           Environment to pass to process (NULL-terminated array).
 * @param flags         Creation behaviour flags.
 * @param priority      Priority class for the process.
 * @param _process      Where to store pointer to new process. If not NULL, the
 *                      process will have a reference on it and must be released
 *                      with process_release() when it is no longer needed.
 *
 * @return              Status code describing result of the operation.
 */
status_t process_create(
    const char *const args[], const char *const env[], uint32_t flags,
    int priority, process_t **_process)
{
    process_load_t load;
    process_t *process;
    thread_t *thread;
    status_t ret;

    assert(args);
    assert(args[0]);
    assert(env);
    assert(priority >= 0 && priority <= PRIORITY_CLASS_MAX);

    load.path = (char *)args[0];
    load.args = (char **)args;
    load.env = (char **)env;

    /* Create the address space for the process. */
    ret = process_load(&load, NULL);
    if (ret != STATUS_SUCCESS)
        return ret;

    token_retain(system_token);

    /* Create the new process. */
    ret = process_alloc(args[0], -1, NULL, priority, load.aspace, system_token, NULL, &process);
    if (ret != STATUS_SUCCESS) {
        vm_aspace_destroy(load.aspace);
        return ret;
    }

    if (flags & PROCESS_CREATE_CRITICAL)
        process->flags |= PROCESS_CRITICAL;

    /* Create and run the entry thread. */
    ret = thread_create("main", process, 0, process_entry_trampoline, &load, NULL, &thread);
    if (ret != STATUS_SUCCESS) {
        process_release(process);
        return ret;
    }

    process->load = &load;
    thread_run(thread);
    thread_release(thread);

    /* Wait for the process to finish loading. */
    semaphore_down(&load.sem);

    if (_process) {
        *_process = process;
    } else {
        process_release(process);
    }

    return load.status;
}

/**
 * Main functions.
 */

/** Dump the contents of the process tree.
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @return              KDB status code. */
static kdb_status_t kdb_cmd_process(int argc, char **argv, kdb_filter_t *filter) {
    process_t *process;

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints a list of all running processes.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("ID     State   Priority Flags Count Running Aspace             Name\n");
    kdb_printf("==     =====   ======== ===== ===== ======= ======             ====\n");

    avl_tree_foreach(&process_tree, iter) {
        process = avl_tree_entry(iter, process_t, tree_link);

        kdb_printf("%-5" PRId32 "%s ", process->id, (process == curr_proc) ? "*" : " ");

        switch (process->state) {
        case PROCESS_CREATED:
            kdb_printf("Created ");
            break;
        case PROCESS_RUNNING:
            kdb_printf("Running ");
            break;
        case PROCESS_DEAD:
            kdb_printf("Dead    ");
            break;
        default:
            kdb_printf("Bad     ");
            break;
        }

        kdb_printf(
            "%-8d %-5d %-5d %-7d %-18p %s\n",
            process->priority, process->flags, refcount_get(&process->count),
            refcount_get(&process->running), process->aspace, process->name);
    }

    return KDB_SUCCESS;;
}

/** Initialize the process table and slab cache. */
__init_text void process_init(void) {
    /* Create the process ID allocator. We reserve ID 0 as it is always
     * given to the kernel process. */
    id_allocator_init(&process_id_allocator, 65535, MM_BOOT);
    id_allocator_reserve(&process_id_allocator, 0);

    /* Create the process slab cache. */
    process_cache = object_cache_create(
        "process_cache",
        process_t, process_ctor, NULL, NULL, 0, MM_BOOT);

    /* Register the KDB command. */
    kdb_register_command("process", "Print a list of running processes.", kdb_cmd_process);

    /* Create the kernel process and register the kernel image to it. */
    token_retain(system_token);
    process_alloc("[kernel]", 0, NULL, PRIORITY_CLASS_SYSTEM, NULL, system_token, NULL, &kernel_proc);
    kernel_proc->flags |= PROCESS_CRITICAL;
    list_append(&kernel_proc->images, &kernel_module.image.header);
}

/** Terminate all running processes. */
void process_shutdown(void) {
    nstime_t interval = 0;
    process_t *process;
    thread_t *thread;
    int count;

    rwlock_read_lock(&process_tree_lock);

    avl_tree_foreach_safe(&process_tree, iter) {
        process = avl_tree_entry(iter, process_t, tree_link);
        if (process != kernel_proc) {
            list_foreach_safe(&process->threads, titer) {
                thread = list_entry(titer, thread_t, owner_link);
                thread_kill(thread);
            }
        }
    }

    rwlock_unlock(&process_tree_lock);

    /* Wait until everything has terminated. */
    do {
        delay(msecs_to_nsecs(1));
        interval += msecs_to_nsecs(1);

        count = 0;
        rwlock_read_lock(&process_tree_lock);

        avl_tree_foreach_safe(&process_tree, iter) {
            process = avl_tree_entry(iter, process_t, tree_link);
            if (process == kernel_proc)
                continue;

            count++;

            if (!(interval % secs_to_nsecs(2)) && process->state == PROCESS_RUNNING) {
                kprintf(
                    LOG_NOTICE, "system: still waiting for %u (%s)...\n",
                    process->id, process->name);
            }
        }

        rwlock_unlock(&process_tree_lock);
    } while (count);

    /* Close the kernel library handle. */
    object_handle_release(kernel_library);
}

/**
 * System calls.
 */

/** Closes a handle to a process.
 * @param handle        Handle to close. */
static void process_object_close(object_handle_t *handle) {
    process_release(handle->private);
}

/** Signal that a process is being waited for.
 * @param handle        Handle to process.
 * @param event         Event being waited for.
 * @return              Status code describing result of the operation. */
static status_t process_object_wait(object_handle_t *handle, object_event_t *event) {
    process_t *process = handle->private;

    switch (event->event) {
    case PROCESS_EVENT_DEATH:
        if (process->state == PROCESS_DEAD) {
            /* For edge-triggered, there's no point adding to the notifier if
             * it's already dead, it won't become dead again. */
            if (!(event->flags & OBJECT_EVENT_EDGE))
                object_event_signal(event, 0);
        } else {
            notifier_register(&process->death_notifier, object_event_notifier, event);
        }

        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_EVENT;
    }
}

/** Stop waiting for a process.
 * @param handle        Handle to process.
 * @param event         Event being waited for. */
static void process_object_unwait(object_handle_t *handle, object_event_t *event) {
    process_t *process = handle->private;

    switch (event->event) {
    case PROCESS_EVENT_DEATH:
        notifier_unregister(&process->death_notifier, object_event_notifier, event);
        break;
    }
}

/** Process object type operations. */
static object_type_t process_object_type = {
    .id = OBJECT_TYPE_PROCESS,
    .flags = OBJECT_TRANSFERRABLE,
    .close = process_object_close,
    .wait = process_object_wait,
    .unwait = process_object_unwait,
};

/** Look up a process given a handle.
 * @param handle        Handle to process, or PROCESS_SELF for calling process.
 * @param _process      Where to store pointer to process (referenced).
 * @return              Status code describing result of the operation. */
static status_t process_handle_lookup(handle_t handle, process_t **_process) {
    object_handle_t *khandle;
    process_t *process;
    status_t ret;

    if (handle == PROCESS_SELF) {
        refcount_inc(&curr_proc->count);

        *_process = curr_proc;
        return STATUS_SUCCESS;
    }

    ret = object_handle_lookup(handle, OBJECT_TYPE_PROCESS, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    process = khandle->private;
    refcount_inc(&process->count);
    object_handle_release(khandle);

    *_process = process;
    return STATUS_SUCCESS;
}

/** Helper to free information copied from userspace.
 * @param load          Structure containing copied information. */
static void free_process_args(process_load_t *load) {
    size_t i;

    if (load->aspace)
        vm_aspace_destroy(load->aspace);

    if (load->token)
        token_release(load->token);

    if (load->root_port)
        ipc_port_release(load->root_port);

    kfree(load->map);

    if (load->env) {
        for (i = 0; load->env[i]; i++)
            kfree(load->env[i]);

        kfree(load->env);
    }

    if (load->args) {
        for (i = 0; load->args[i]; i++)
            kfree(load->args[i]);

        kfree(load->args);
    }

    kfree(load->path);
}

/** Helper to copy process creation information from userspace.
 * @param path          Path to copy.
 * @param args          Argument array to copy.
 * @param env           Environment array to copy.
 * @param attrib        Attributes structure.
 * @param load          Pointer to information structure to fill in.
 * @return              Status code describing result of the operation. */
static status_t copy_process_args(
    const char *path, const char *const args[], const char *const env[],
    const process_attrib_t *attrib, process_load_t *load)
{
    process_attrib_t kattrib;
    object_handle_t *handle;
    size_t size;
    status_t ret;

    if (!path || !args || !env)
        return STATUS_INVALID_ARG;

    memset(load, 0, sizeof(*load));

    ret = strndup_from_user(path, FS_PATH_MAX, &load->path);
    if (ret != STATUS_SUCCESS)
        goto err;

    ret = arrcpy_from_user(args, &load->args);
    if (ret != STATUS_SUCCESS)
        goto err;

    ret = arrcpy_from_user(env, &load->env);
    if (ret != STATUS_SUCCESS)
        goto err;

    if (attrib) {
        ret = memcpy_from_user(&kattrib, attrib, sizeof(kattrib));
        if (ret != STATUS_SUCCESS)
            goto err;

        if (kattrib.map_count > 0 && !kattrib.map) {
            ret = STATUS_INVALID_ARG;
            goto err;
        }

        load->map_count = kattrib.map_count;
        if (load->map_count > 0) {
            size = sizeof(handle_t) * 2 * load->map_count;
            load->map = kmalloc(size, MM_USER);
            if (!load->map) {
                ret = STATUS_NO_MEMORY;
                goto err;
            }

            ret = memcpy_from_user(load->map, kattrib.map, size);
            if (ret != STATUS_SUCCESS)
                goto err;
        }

        if (kattrib.token >= 0) {
            ret = object_handle_lookup(kattrib.token, OBJECT_TYPE_TOKEN, &handle);
            if (ret != STATUS_SUCCESS)
                goto err;

            load->token = handle->private;
            token_retain(load->token);
            object_handle_release(handle);
        }

        if (kattrib.root_port >= 0) {
            ret = object_handle_lookup(kattrib.root_port, OBJECT_TYPE_PORT, &handle);
            if (ret != STATUS_SUCCESS)
                goto err;

            load->root_port = handle->private;
            ipc_port_retain(load->root_port);
            object_handle_release(handle);
        }
    } else {
        load->map_count = -1;
    }

    if (!load->token)
        load->token = token_inherit(curr_proc->token);

    if (!load->root_port && curr_proc->root_port) {
        load->root_port = curr_proc->root_port;
        ipc_port_retain(load->root_port);
    }

    return STATUS_SUCCESS;

err:
    free_process_args(load);
    return ret;
}

/**
 * Create a new process.
 *
 * Creates a new process and executes a program within it, and returns a handle
 * to it. The handle can be used to query information about the new process,
 * and wait for it to terminate. It should be closed as soon as it is no longer
 * needed so that the process can be freed once it has exited. An optional
 * attributes structure can be provided which specifies additional attributes
 * for the new process. See the documentation for process_attrib_t for the
 * default behaviour when this structure is not provided.
 *
 * If a handle to a security token is given, the security context for the new
 * process will be set to the context contained in that token. Otherwise, the
 * new process will have the same identity as the calling process, and both its
 * effective and inheritable privilege set will be set to the calling process'
 * inheritable privilege set.
 *
 * If no handle map is specified in the attributes structure, then all
 * inheritable handle table entries in the calling process will be duplicated
 * into the child process with the same IDs. Otherwise, handles will be
 * duplicated according to the map, see the documentation for process_attrib_t
 * for details.
 *
 * @param path          Path to binary to load.
 * @param args          NULL-terminated array of arguments to the program.
 * @param env           NULL-terminated array of environment variables.
 * @param flags         Flags modifying creation behaviour.
 * @param attrib        Optional attributes structure.
 * @param _handle       Where to store handle to process (can be NULL).
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_process_create(
    const char *path, const char *const args[], const char *const env[],
    uint32_t flags, const process_attrib_t *attrib, handle_t *_handle)
{
    process_load_t load;
    object_handle_t *khandle;
    process_t *process;
    handle_t uhandle;
    thread_t *thread;
    status_t ret;

    /* Marking a process as critical causes a fatal error if it exits, so
     * require PRIV_FATAL. */
    if (flags & PROCESS_CREATE_CRITICAL) {
        if (!security_check_priv(PRIV_FATAL))
            return STATUS_PERM_DENIED;
    }

    ret = copy_process_args(path, args, env, attrib, &load);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Create the address space for the process. */
    ret = process_load(&load, curr_proc);
    if (ret != STATUS_SUCCESS)
        goto out_free_args;

    /* Create the new process. */
    ret = process_alloc(
        args[0], -1, curr_proc, curr_proc->priority, load.aspace, load.token,
        load.root_port, &process);
    if (ret != STATUS_SUCCESS)
        goto out_free_args;

    /* These will now be freed when destroying the process. */
    load.aspace = NULL;
    load.token = NULL;
    load.root_port = NULL;

    if (flags & PROCESS_CREATE_CRITICAL)
        process->flags |= PROCESS_CRITICAL;

    /* Duplicate handles into the new process. */
    ret = object_process_create(process, curr_proc, load.map, load.map_count);
    if (ret != STATUS_SUCCESS)
        goto out_release_process;

    /* Create a handle if necessary. */
    if (_handle) {
        refcount_inc(&process->count);

        khandle = object_handle_create(&process_object_type, process);
        ret = object_handle_attach(khandle, &uhandle, _handle);
        object_handle_release(khandle);
        if (ret != STATUS_SUCCESS)
            goto out_release_process;
    }

    /* Create and run the entry thread. */
    ret = thread_create("main", process, 0, process_entry_trampoline, &load, NULL, &thread);
    if (ret != STATUS_SUCCESS) {
        if (_handle)
            object_handle_detach(uhandle);

        goto out_release_process;
    }

    process->load = &load;
    thread_run(thread);
    thread_release(thread);

    /* Wait for the process to finish loading. */
    semaphore_down(&load.sem);

    ret = load.status;
    if (ret != STATUS_SUCCESS && _handle)
        object_handle_detach(uhandle);

out_release_process:
    process_release(process);

out_free_args:
    free_process_args(&load);
    return ret;
}

/**
 * Replace the current process with a new program.
 *
 * Replaces the current process with a new program. All threads in the process
 * other than the calling thread will be terminated. Upon successful completion,
 * the process will be in the same state as a new process would be after a
 * call to kern_process_create() with the same arguments, except it retains the
 * same process ID and all handles open to the process remain valid.
 *
 * If a handle to a security token is given, the security context for the
 * process will be set to the context contained in that token. Otherwise, the
 * process will keep the same identity, and its effective privilege set will be
 * set to its inheritable privilege set.
 *
 * If no handle map is specified in the attributes structure, then all
 * inheritable handle table entries in the process will remain open with the
 * same IDs. Otherwise, handles will be made available in the new program
 * according to the map, see the documentation for process_attrib_t for details.
 *
 * @param path          Path to binary to load.
 * @param args          NULL-terminated array of arguments to the program.
 * @param env           NULL-terminated array of environment variables.
 * @param flags         Flags modifying creation behaviour.
 * @param attrib        Optional attributes structure.
 *
 * @return              Does not return on success, returns status code on
 *                      failure.
 */
status_t kern_process_exec(
    const char *path, const char *const args[], const char *const env[],
    uint32_t flags, const process_attrib_t *attrib)
{
    process_load_t load;
    thread_t *thread = NULL;
    char *name;
    status_t ret;

    if (flags & PROCESS_CREATE_CRITICAL) {
        if (!security_check_priv(PRIV_FATAL))
            return STATUS_PERM_DENIED;
    }

    if (curr_proc->threads.next->next != &curr_proc->threads) {
        kprintf(LOG_WARN, "kern_process_exec: TODO: Terminate other threads\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    ret = copy_process_args(path, args, env, attrib, &load);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Create the new address space for the process. */
    ret = process_load(&load, curr_proc);
    if (ret != STATUS_SUCCESS)
        goto err_free_args;

    /* Create the entry thread to finish loading the program. */
    ret = thread_create("main", curr_proc, 0, process_entry_trampoline, &load, NULL, &thread);
    if (ret != STATUS_SUCCESS)
        goto err_free_args;

    ret = object_process_exec(load.map, load.map_count);
    if (ret != STATUS_SUCCESS)
        goto err_release_thread;

    mutex_lock(&curr_proc->lock);

    if (flags & PROCESS_CREATE_CRITICAL) {
        curr_proc->flags |= PROCESS_CRITICAL;
    } else {
        curr_proc->flags &= ~PROCESS_CRITICAL;
    }

    /* Mark our own stack as NULL so that the thread cleanup code will not try
     * to unmap it when we exit. Doing so would end up unmapping something in
     * the new address space. */
    curr_thread->ustack = 0;
    curr_thread->ustack_size = 0;

    /* Switch over to the new address space. */
    preempt_disable();
    vm_aspace_switch(load.aspace);
    swap(curr_proc->aspace, load.aspace);
    swap(curr_proc->token, load.token);
    name = curr_proc->name;
    curr_proc->name = load.path;
    preempt_enable();

    /* Free all currently loaded images. */
    elf_process_cleanup(curr_proc);

    mutex_unlock(&curr_proc->lock);

    /* Run the thread and wait for it to complete. */
    thread_run(thread);
    thread_release(thread);
    semaphore_down(&load.sem);

    /* Free up old process information. We swapped the address space, token and
     * root port pointers into the load structure, so they will get freed by
     * free_process_args(). Don't swap name until now because it is used by
     * process_entry_trampoline(). */
    load.path = name;
    free_process_args(&load);
    thread_exit();

err_release_thread:
    thread_release(thread);

err_free_args:
    free_process_args(&load);
    return ret;
}

/** Entry point for a cloned process.
 * @param arg1          Pointer to cloned frame.
 * @param arg2          User-specified handle location. */
static void process_clone_trampoline(void *arg1, void *arg2) {
    frame_t frame;

    /* Set the user's handle to INVALID_HANDLE for it to determine that it is
     * the child process. This should succeed as in the parent process we wrote
     * the address successfully. */
    write_user((handle_t *)arg2, INVALID_HANDLE);

    /* Copy the allocated frame onto the kernel stack and free it. */
    memcpy(&frame, arg1, sizeof(frame));
    kfree(arg1);

    arch_thread_user_enter(&frame);
}

/**
 * Clone the calling process.
 *
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to the pages. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process' security context will
 * be identical to the parent's. The new process will inherit all handles to
 * transferrable objects from the parent, including ones not marked as
 * inheritable (non-inheritable handles are only closed when a new program is
 * executed with kern_process_exec() or kern_process_create()).
 *
 * Threads other than the calling thread are NOT cloned. The new process will
 * have a single thread which will resume execution after the call to
 * kern_process_clone().
 *
 * @param _handle       In the parent process, the location pointed to will be
 *                      set to a handle to the child process upon success. In
 *                      the child process, it will be set to INVALID_HANDLE.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_process_clone(handle_t *_handle) {
    vm_aspace_t *as;
    process_t *process;
    object_handle_t *khandle;
    handle_t uhandle;
    frame_t *frame;
    thread_t *thread;
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    /* Clone the address space, reference other things we're inheriting. */
    as = vm_aspace_clone(curr_proc->aspace);
    token_retain(curr_proc->token);
    if (curr_proc->root_port)
        ipc_port_retain(curr_proc->root_port);

    ret = process_alloc(
        curr_proc->name, -1, curr_proc, curr_proc->priority, as,
        curr_proc->token, curr_proc->root_port, &process);
    if (ret != STATUS_SUCCESS) {
        if (curr_proc->root_port)
            ipc_port_release(curr_proc->root_port);

        token_release(curr_proc->token);
        vm_aspace_destroy(as);
        return ret;
    }

    mutex_lock(&curr_proc->lock);

    /* Clone handles and other per-process information. */
    object_process_clone(process, curr_proc);
    elf_process_clone(process, curr_proc);
    memcpy(&process->exceptions, &curr_proc->exceptions, sizeof(process->exceptions));

    mutex_unlock(&curr_proc->lock);

    /* Create a new handle. This takes over the initial reference added by
     * process_alloc(). */
    khandle = object_handle_create(&process_object_type, process);
    ret = object_handle_attach(khandle, &uhandle, _handle);
    if (ret != STATUS_SUCCESS) {
        object_handle_release(khandle);
        return ret;
    }

    /* Create the entry thread. */
    frame = kmalloc(sizeof(*frame), MM_KERNEL);
    ret = thread_create(
        curr_thread->name, process, 0, process_clone_trampoline, frame, _handle,
        &thread);
    object_handle_release(khandle);
    if (ret != STATUS_SUCCESS) {
        kfree(frame);
        object_handle_detach(uhandle);
        return ret;
    }

    /* Clone arch-specific thread attributes and get the frame to restore. */
    spinlock_lock(&curr_thread->lock);
    arch_thread_clone(thread, frame);
    spinlock_unlock(&curr_thread->lock);

    /* Inherit other per-thread attributes from the calling thread. */
    memcpy(&thread->exceptions, &curr_thread->exceptions, sizeof(thread->exceptions));
    thread->ustack = curr_thread->ustack;
    thread->ustack_size = curr_thread->ustack_size;

    thread_run(thread);
    thread_release(thread);
    return STATUS_SUCCESS;
}

/** Open a handle to a process.
 * @param id            ID of the process to open.
 * @param _handle       Where to store handle to process.
 * @return              Status code describing result of the operation. */
status_t kern_process_open(process_id_t id, handle_t *_handle) {
    process_t *process;
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    process = process_lookup(id);
    if (!process)
        return STATUS_NOT_FOUND;

    if (process == kernel_proc) {
        process_release(process);
        return STATUS_NOT_FOUND;
    }

    /* Reference added by process_lookup() is taken over by this handle. */
    ret = object_handle_open(&process_object_type, process, NULL, _handle);
    if (ret != STATUS_SUCCESS)
        process_release(process);

    return ret;
}

/** Get the ID of a process.
 * @param handle        Handle to process, or PROCESS_SELF for calling process.
 * @return              Process ID on success, -1 if handle is invalid. */
process_id_t kern_process_id(handle_t handle) {
    process_id_t id = -1;
    process_t *process;
    status_t ret;

    ret = process_handle_lookup(handle, &process);
    if (ret == STATUS_SUCCESS) {
        id = process->id;
        process_release(process);
    }

    return id;
}

/**
 * Get a process' security context.
 *
 * Gets the given process' security context. This is only useful to query a
 * process' current identity, as it returns only the context content, rather
 * than a token object containing it. No special privilege is required to get
 * a process' security context.
 *
 * @param handle        Handle to process, or PROCESS_SELF for calling process.
 * @param ctx           Where to store security context of the process.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_process_security(handle_t handle, security_context_t *ctx) {
    process_t *process;
    token_t *token;
    status_t ret;

    if (!ctx)
        return STATUS_INVALID_ARG;

    ret = process_handle_lookup(handle, &process);
    if (ret != STATUS_SUCCESS)
        return ret;

    mutex_lock(&process->lock);

    token = process->token;
    token_retain(token);

    mutex_unlock(&process->lock);

    ret = memcpy_to_user(ctx, &token->ctx, sizeof(token->ctx));

    token_release(token);
    process_release(process);
    return ret;
}

/**
 * Get one of a process' special ports.
 *
 * Gets a handle to one of a process' special ports. The calling thread must
 * have privileged access to the process, i.e. the user IDs must match, or it
 * must have the PRIV_PROCESS_ADMIN privilege.
 *
 * @param handle        Handle to process, or PROCESS_SELF for calling process.
 * @param id            Special port ID.
 * @param _handle       Where to store handle to port.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if process handle is invalid.
 *                      STATUS_ACCESS_DENIED if calling thread does not have
 *                      sufficient privileges to access the process' special
 *                      ports.
 *                      STATUS_INVALID_ARG if port ID is invalid.
 *                      STATUS_NOT_FOUND if port does not exist.
 */
status_t kern_process_port(handle_t handle, int32_t id, handle_t *_handle) {
    process_t *process;
    ipc_port_t *port;
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    ret = process_handle_lookup(handle, &process);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (!process_access(process)) {
        ret = STATUS_ACCESS_DENIED;
        goto out;
    }

    switch (id) {
    case PROCESS_ROOT_PORT:
        if (!process->root_port) {
            ret = STATUS_NOT_FOUND;
            goto out;
        }

        port = process->root_port;
        break;
    default:
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    ret = ipc_port_publish(port, NULL, _handle);

out:
    process_release(process);
    return ret;
}

/**
 * Query the status of a process. Returns whether the process is still running,
 * and optionally its exit status/reason. Querying exit status code and reason
 * requires privileged access to the process, but just querying whether the
 * process is running (_status and _reason are both NULL) is allowed from any
 * process.
 *
 * @param handle        Handle to process.
 * @param _status       Where to store exit status of process (can be NULL).
 * @param _reason       Where to store exit reason (can be NULL).
 *
 * @return              STATUS_SUCCESS if the process is dead.
 *                      STATUS_STILL_RUNNING if the process is running.
 *                      STATUS_ACCESS_DENIED if _status or _reason are not null
 *                      and the caller does not have privileged access to the
 *                      process.
 *                      STATUS_INVALID_HANDLE if handle is invalid.
 */
status_t kern_process_status(handle_t handle, int *_status, int *_reason) {
    process_t *process;
    status_t ret;

    /* Although getting the status of the current process is silly (it'll error
     * below), support it anyway for consistency's sake. */
    ret = process_handle_lookup(handle, &process);
    if (ret != STATUS_SUCCESS)
        return ret;

    if ((_status || _reason) && !process_access(process)) {
        ret = STATUS_ACCESS_DENIED;
    } else if (process->state != PROCESS_DEAD) {
        ret = STATUS_STILL_RUNNING;
    }

    if (ret == STATUS_SUCCESS && _status)
        ret = write_user(_status, process->status);

    if (ret == STATUS_SUCCESS && _reason)
        ret = write_user(_reason, process->reason);

    process_release(process);
    return ret;
}

/**
 * Kill a process.
 *
 * Kill the process (i.e. cause it to immediately exit) referred to by the
 * specified handle. The calling thread must have privileged access to the
 * process.
 *
 * @param handle        Handle to process.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is invalid.
 *                      STATUS_ACCESS_DENIED if the caller does not have
 *                      privileged access to the process.
 */
status_t kern_process_kill(handle_t handle) {
    process_t *process;
    thread_t *thread;
    status_t ret;

    ret = process_handle_lookup(handle, &process);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (!process_access(process)) {
        ret = STATUS_ACCESS_DENIED;
    } else {
        mutex_lock(&process->lock);

        process->reason = EXIT_REASON_KILLED;

        /* Kill all of the process' threads. If this is the current process, we
         * will exit when returning back to user mode. */
        list_foreach_safe(&process->threads, iter) {
            thread = list_entry(iter, thread_t, owner_link);
            thread_kill(thread);
        }

        mutex_unlock(&process->lock);
    }

    process_release(process);
    return ret;
}

/** Get the calling process' security token.
 * @param _handle       Where to store handle to token.
 * @return              Status code describing the result of the operation. */
status_t kern_process_token(handle_t *_handle) {
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    mutex_lock(&curr_proc->lock);
    ret = token_publish(curr_proc->token, NULL, _handle);
    mutex_unlock(&curr_proc->lock);

    return ret;
}

/**
 * Set the calling process' security token.
 *
 * Sets the calling process' security token to the given token object. The
 * process will take on the identity given by the security context held in the
 * token, and the context will be used for any future security checks in the
 * process. If any threads currently have an overridden token set, they will
 * continue to use that until it is unset, at which point they will start using
 * the new token set by this function.
 *
 * @param handle        Handle to token.
 *
 * @return              Status code describing the result of the operation.
 */
status_t kern_process_set_token(handle_t handle) {
    object_handle_t *khandle;
    token_t *token;
    status_t ret;

    ret = object_handle_lookup(handle, OBJECT_TYPE_TOKEN, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    token = khandle->private;
    token_retain(token);
    object_handle_release(khandle);

    mutex_lock(&curr_proc->lock);
    token_release(curr_proc->token);
    curr_proc->token = token;
    mutex_unlock(&curr_proc->lock);
    return STATUS_SUCCESS;
}

/**
 * Set an exception handler.
 *
 * Set a process-wide exception handler. In addition to the process-wide set of
 * handlers, each thread has its own set of handlers. If a per-thread handler
 * is set, it is used over the process-wide handler when an exception occurs.
 * If there is neither a per-thread handler or a process-wide handler for an
 * exception that occurs, the whole process is killed.
 *
 * @param code          Exception to set handler for.
 * @param handler       Handler function to use (NULL to unset the process-wide
 *                      handler).
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if code is invalid.
 *                      STATUS_INVALID_ADDR if handler is an invalid address.
 */
status_t kern_process_set_exception_handler(unsigned code, exception_handler_t handler) {
    if (code >= EXCEPTION_MAX) {
        return STATUS_INVALID_ARG;
    } else if (handler && !is_user_address(handler)) {
        return STATUS_INVALID_ADDR;
    }

    /* Locking is necessary here due to the memcpy() of the exception table in
     * kern_process_clone(). Setting should be atomic. */
    mutex_lock(&curr_proc->lock);
    curr_proc->exceptions[code] = handler;
    mutex_unlock(&curr_proc->lock);
    return STATUS_SUCCESS;
}

/**
 * Terminate the calling process.
 *
 * Terminates the calling process. All threads in the process will also be
 * terminated. The status code given can be retrieved by any processes with a
 * handle to the process.
 *
 * @param status            Exit status code.
 */
void kern_process_exit(int status) {
    curr_proc->status = status;
    process_exit();
}

/** Perform operations on the current process (for internal use by libkernel).
 * @param action        Action to perform.
 * @param in            Pointer to input buffer.
 * @param out           Pointer to output buffer.
 * @return              Status code describing result of the operation. */
status_t kern_process_control(unsigned action, const void *in, void *out) {
    switch (action) {
    case PROCESS_LOADED:
        mutex_lock(&curr_proc->lock);

        if (curr_proc->load) {
            curr_proc->load->status = STATUS_SUCCESS;
            semaphore_up(&curr_proc->load->sem, 1);
            curr_proc->load = NULL;
        }

        mutex_unlock(&curr_proc->lock);
        return STATUS_SUCCESS;
    case PROCESS_SET_RESTORE:
        if (!is_user_address(in))
            return STATUS_INVALID_ADDR;

        curr_proc->thread_restore = (ptr_t)in;
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_ARG;
    }
}
