/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Security tokens.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <security/security.h>
#include <security/token.h>

#include <kernel.h>
#include <status.h>

/** Cache for token objects. */
static slab_cache_t *token_cache;

/** Fully privileged token used by the kernel and initial user process. */
token_t *system_token;

/** Closes a handle to a token. */
static void token_object_close(object_handle_t *handle) {
    token_release(handle->private);
}

/** Token object type. */
static const object_type_t token_object_type = {
    .id    = OBJECT_TYPE_TOKEN,
    .flags = OBJECT_TRANSFERRABLE,
    .close = token_object_close,
};

/** Increase the reference count of a token.
 * @param token         Token to increase the reference count of. */
void token_retain(token_t *token) {
    refcount_inc(&token->count);
}

/** Decrease the reference count of a token.
 * @param token         Token to decrease the reference count of. */
void token_release(token_t *token) {
    if (refcount_dec(&token->count) == 0)
        slab_cache_free(token_cache, token);
}

/**
 * Inherits a token for a newly created process. If possible, the token is
 * shared, in which case the reference count will be increased. Otherwise, a
 * copy will be created.
 *
 * @param source        Token to inherit.
 *
 * @return              Pointer to token to use for new process.
 */
token_t *token_inherit(token_t *source) {
    if (source->copy_on_inherit) {
        token_t *token = slab_cache_alloc(token_cache, MM_KERNEL);
        refcount_set(&token->count, 1);

        token->ctx.uid = source->ctx.uid;
        token->ctx.gid = source->ctx.gid;

        memcpy(&token->ctx.groups, &source->ctx.groups, sizeof(token->ctx.groups));

        /* Both the effective and inheritable sets should be set to the source's
         * inheritable set. */
        memcpy(&token->ctx.privs, &source->ctx.inherit, sizeof(token->ctx.privs));
        memcpy(&token->ctx.inherit, &source->ctx.inherit, sizeof(token->ctx.inherit));

        return token;
    } else {
        token_retain(source);
        return source;
    }
}

/**
 * Gets the current thread's active security token. A thread's active security
 * token remains constant for the entire time that the thread is in the kernel,
 * i.e. if another thread changes the process-wide security context, the change
 * will not take effect until the current thread returns to userspace. The
 * returned token does not have an extra reference added, it remains valid
 * until the calling thread exits the kernel. If it needs to be kept after this,
 * the token must be explicitly referenced.
 *
 * @return              Currently active security token.
 */
token_t *token_current(void) {
    token_t *token;

    if (curr_thread->active_token) {
        token = curr_thread->active_token;
    } else {
        mutex_lock(&curr_proc->lock);

        token = (curr_thread->token) ? curr_thread->token : curr_proc->token;
        token_retain(token);

        mutex_unlock(&curr_proc->lock);

        /* Save the active token to be returned by subsequent calls. An
         * alternative to doing this would be to always save the token in
         * thread_at_kernel_entry(), however doing so would be inefficient: it
         * would require a process lock on every kernel entry, and for a lot of
         * kernel entries the security token is not required. By saving the
         * token the first time we call this function, we still achieve the
         * desired behaviour of not having the thread's identity change while
         * doing security checks. */
        curr_thread->active_token = token;
    }

    return token;
}

/**
 * Creates a handle to a token and publishes it in the current process' handle
 * table. A new reference will be added to the token.
 *
 * @param token         Token to publish.
 * @param _id           If not NULL, a kernel location to store handle ID in.
 * @param _uid          If not NULL, a user location to store handle ID in.
 */
status_t token_publish(token_t *token, handle_t *_id, handle_t *_uid) {
    token_retain(token);

    object_handle_t *handle = object_handle_create(&token_object_type, token);
    status_t ret = object_handle_attach(handle, _id, _uid);
    object_handle_release(handle);
    return ret;
}

/** Initializes the security token allocator. */
__init_text void token_init(void) {
    token_cache = object_cache_create("token_cache", token_t, NULL, NULL, NULL, 0, MM_BOOT);

    /* Create the system token. It is granted all privileges. */
    system_token = slab_cache_alloc(token_cache, MM_BOOT);
    refcount_set(&system_token->count, 1);
    system_token->ctx.uid = 0;
    system_token->ctx.gid = 0;
    for (size_t i = 0; i < SECURITY_CONTEXT_MAX_GROUPS; i++)
        system_token->ctx.groups[i] = -1;
    for (size_t i = 0; i <= PRIV_MAX; i++) {
        security_context_set_priv(&system_token->ctx, i);
        security_context_set_inherit(&system_token->ctx, i);
    }
}

static int compare_group(const void *a, const void *b) {
    group_id_t ga = *(const group_id_t *)a;
    group_id_t gb = *(const group_id_t *)b;

    /* This forces negative entries to be last in the array. */
    if ((ga < 0 && gb < 0) || (ga >= 0 && gb >= 0)) {
        return ga - gb;
    } else if (ga < 0) {
        return 1;
    } else {
        return -1;
    }
}

/**
 * Creates a new security token encapsulating the given security context. The
 * calling thread must have have the necessary privileges to create the token.
 * Unless the thread has the PRIV_CHANGE_IDENTITY privilege, the user ID and
 * group IDs must match the thread's current user ID and group IDs. The context
 * cannot contain any privileges that the thread does not currently have, and
 * the inheritable privilege set must be a subset of the effective privilege
 * set.
 *
 * @param ctx           Security context to use.
 * @param _handle       Where to store handle to created token.
 */
status_t kern_token_create(const security_context_t *ctx, handle_t *_handle) {
    status_t ret;

    token_t *creator = token_current();

    token_t *token = slab_cache_alloc(token_cache, MM_KERNEL);
    refcount_set(&token->count, 0);

    token->copy_on_inherit = false;

    ret = memcpy_from_user(&token->ctx, ctx, sizeof(token->ctx));
    if (ret != STATUS_SUCCESS)
        goto err_free_token;

    if (token->ctx.uid < 0 || token->ctx.gid < 0) {
        ret = STATUS_INVALID_ARG;
        goto err_free_token;
    }

    /* Sort the supplementary groups array into ascending order, with all unused
     * (negative) entries toward the end. This makes identity comparison easy:
     * we can just use memcmp(). */
    qsort(
        token->ctx.groups, array_size(token->ctx.groups),
        sizeof(token->ctx.groups[0]), compare_group);

    /* Mask out unsupported bits. */
    for (size_t i = PRIV_MAX + 1; i < SECURITY_CONTEXT_MAX_PRIVS; i++) {
        security_context_unset_priv(&token->ctx, i);
        security_context_unset_inherit(&token->ctx, i);
    }

    for (size_t i = 0; i < array_size(token->ctx.privs); i++) {
        /* The inheritable set must be a subset of the effective set. */
        if (token->ctx.inherit[i] & ~(token->ctx.privs[i])) {
            ret = STATUS_INVALID_ARG;
            goto err_free_token;
        }

        /* Need to copy the token when inheriting if the inherit set is not the
         * same as the effective set. */
        if (token->ctx.inherit[i] != token->ctx.privs[i])
            token->copy_on_inherit = true;
    }

    /* Cannot set privileges that the creator does not have. */
    for (size_t i = 0; i < array_size(token->ctx.privs); i++) {
        if (token->ctx.privs[i] & ~(creator->ctx.privs[i])) {
            ret = STATUS_PERM_DENIED;
            goto err_free_token;
        }
    }

    /* If we do not have PRIV_CHANGE_IDENTITY, we cannot change identity. */
    if (!security_check_priv(PRIV_CHANGE_IDENTITY)) {
        if (token->ctx.uid != creator->ctx.uid ||
            token->ctx.gid != creator->ctx.gid ||
            memcmp(token->ctx.groups, creator->ctx.groups, sizeof(token->ctx.groups)))
        {
            ret = STATUS_PERM_DENIED;
            goto err_free_token;
        }
    }

    /* Will free the token on failure because we initialised the count to 0. */
    return token_publish(token, NULL, _handle);

err_free_token:
    slab_cache_free(token_cache, token);
    return ret;
}

/** Retrieves the security context held by a token.
 * @param handle        Handle to security token.
 * @param ctx           Where to store security context. */
status_t kern_token_query(handle_t handle, security_context_t *ctx) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_TOKEN, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    token_t *token = khandle->private;

    ret = memcpy_to_user(ctx, &token->ctx, sizeof(token->ctx));
    object_handle_release(khandle);
    return ret;
}
