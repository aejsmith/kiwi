/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Thread functions.
 */

#include <kernel/futex.h>
#include <kernel/object.h>
#include <kernel/private/thread.h>

#include <inttypes.h>

#include "libkernel.h"

/** Information used by thread_create(). */
typedef struct thread_create {
    volatile int32_t futex;         /**< Futex to wait on. */
    tls_tcb_t *tcb;                 /**< TLS thread control block. */
    thread_entry_t entry;           /**< Real entry point. */
    void *arg;                      /**< Real entry point argument. */
} thread_create_t;

/** Saved ID for the current thread. */
__thread thread_id_t curr_thread_id = -1;

/** Thread destructor functions. */
#define THREAD_DTOR_MAX 8
static thread_dtor_t thread_dtors[THREAD_DTOR_MAX] = {};

/** Thread entry wrapper.
 * @param _create       Pointer to information structure. */
static int thread_trampoline(void *_create) {
    thread_create_t *create = _create;

    thread_id_t id;
    _kern_thread_id(THREAD_SELF, &id);

    /* Set our TCB. */
    dprintf("tls: TCB for thread %" PRId32 " is %p\n", id, create->tcb);
    kern_thread_control(THREAD_SET_TLS_ADDR, create->tcb, NULL);

    /* Save our ID. */
    curr_thread_id = id;

    /* After we unblock the creating thread, create is no longer valid. */
    thread_entry_t entry = create->entry;
    void *arg            = create->arg;

    /* Unblock our creator. */
    create->futex = 1;
    kern_futex_wake((int32_t *)&create->futex, 1, NULL);

    /* Call the real entry point. */
    kern_thread_exit(entry(arg));
}

__sys_export status_t kern_thread_create(
    const char *name, thread_entry_t entry, void *arg,
    const thread_stack_t *stack, uint32_t flags, handle_t *_handle)
{
    status_t ret;

    if (!entry)
        return STATUS_INVALID_ARG;

    thread_create_t create;
    create.futex = 0;
    create.entry = entry;
    create.arg   = arg;

    /* Allocate a TLS block. */
    ret = tls_alloc(&create.tcb);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Create the thread. */
    ret = _kern_thread_create(name, thread_trampoline, &create, stack, flags, _handle);
    if (ret != STATUS_SUCCESS) {
        tls_destroy(create.tcb);
        return ret;
    }

    /* Wait for the thread to complete TLS setup. TODO: There is a possible bug
     * here: if the thread somehow ends up killed before it wakes us we will get
     * stuck. We should create an event object instead and wait on both that and
     * the thread so we get woken if the thread dies. */
    kern_futex_wait((int32_t *)&create.futex, 0, -1);
    return STATUS_SUCCESS;
}

__sys_export status_t kern_thread_id(handle_t handle, thread_id_t *_id) {
    /* We save the current thread ID to avoid having to perform a kernel call
     * just to get our own ID. */
    if (handle == THREAD_SELF) {
        *_id = curr_thread_id;
        return STATUS_SUCCESS;
    } else {
        return _kern_thread_id(handle, _id);
    }
}

__sys_export void kern_thread_exit(int status) {
    tls_tcb_t *tcb = arch_tls_tcb();

    for (size_t i = 0; i < THREAD_DTOR_MAX; i++) {
        if (thread_dtors[i])
            thread_dtors[i]();
    }

    dprintf("tls: destroying block %p for thread %" PRId32 "\n", tcb->base, curr_thread_id);

    tls_destroy(tcb);

    _kern_thread_exit(status);
}

/**
 * Add a destructor function to be called whenever a thread exits. If the
 * function already exists in the list then it will not be added again.
 *
 * @param dtor          Destructor function.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NO_MEMORY if there is no space in the destructor
 *                      list.
 */
__sys_export status_t kern_thread_add_dtor(thread_dtor_t dtor) {
    size_t first_free = THREAD_DTOR_MAX;
    for (size_t i = 0; i < THREAD_DTOR_MAX; i++) {
        if (thread_dtors[i] == dtor) {
            return STATUS_SUCCESS;
        } else if (!thread_dtors[i] && first_free == THREAD_DTOR_MAX) {
            first_free = i;
        }
    }

    if (first_free < THREAD_DTOR_MAX) {
        thread_dtors[first_free] = dtor;
        return STATUS_SUCCESS;
    } else {
        return STATUS_NO_MEMORY;
    }
}
