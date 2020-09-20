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
 * @brief               Kernel security definitions.
 */

#pragma once

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of groups a process can be in. */
#define SECURITY_CONTEXT_MAX_GROUPS 32

/** Maximum number of privileges. */
#define SECURITY_CONTEXT_MAX_PRIVS  128

/** Definitions of privileges. */
#define PRIV_SHUTDOWN               0   /**< Ability to shut down the system. */
#define PRIV_FATAL                  1   /**< Ability to halt the kernel with a fatal error. */
#define PRIV_MODULE                 2   /**< Ability to load/unload kernel modules. */
#define PRIV_CHANGE_IDENTITY        3   /**< Ability to change user/group IDs. */
#define PRIV_CHANGE_OWNER           4   /**< Ability to set object user/group to arbitrary IDs. */
#define PRIV_FS_ADMIN               5   /**< Ability to bypass access checks on the filesystem. */
#define PRIV_FS_SETROOT             6   /**< Ability to use the fs_setroot() system call. */
#define PRIV_FS_MOUNT               7   /**< Ability to mount/unmount filesystems. */
#define PRIV_PROCESS_ADMIN          8   /**< Ability to control any process/thread. */

/** Currently highest defined privilege. */
#define PRIV_MAX                    8

/** Structure defining the security context for a process/thread. */
typedef struct security_context {
    /** User ID. */
    user_id_t uid;

    /** Primary group ID. */
    group_id_t gid;

    /** Supplementary group IDs (unused entries should be set negative). */
    group_id_t groups[SECURITY_CONTEXT_MAX_GROUPS];

    /** Effective privileges bitmap. */
    uint32_t privs[SECURITY_CONTEXT_MAX_PRIVS / 32];

    /** Inheritable privileges bitmap. */
    uint32_t inherit[SECURITY_CONTEXT_MAX_PRIVS / 32];
} security_context_t;

/** Initialize a security context.
 * @param ctx           Context to intialize. */
static inline void security_context_init(security_context_t *ctx) {
    size_t i;

    ctx->uid = 0;
    ctx->gid = 0;

    for (i = 0; i < SECURITY_CONTEXT_MAX_GROUPS; i++)
        ctx->groups[i] = -1;

    for (i = 0; i < (SECURITY_CONTEXT_MAX_PRIVS / 32); i++) {
        ctx->privs[i] = 0;
        ctx->inherit[i] = 0;
    }
}

/** Check if a security context is a member of a group.
 * @param ctx           Context to check.
 * @param gid           Group ID to check for.
 * @return              Whether the context has the specified group. */
static inline bool security_context_has_group(const security_context_t *ctx, group_id_t gid) {
    size_t i;

    if (ctx->gid == gid)
        return true;

    for (i = 0; i < SECURITY_CONTEXT_MAX_GROUPS; i++) {
        if (ctx->groups[i] >= 0 && ctx->groups[i] == gid)
            return true;
    }

    return false;
}

/** Add a supplementary group to a security context.
 * @param ctx           Context to add to.
 * @param gid           Group ID to add.
 * @return              True if group was added, false if table full. */
static inline bool security_context_add_group(security_context_t *ctx, group_id_t gid) {
    size_t i;

    for (i = 0; i < SECURITY_CONTEXT_MAX_GROUPS; i++) {
        if (ctx->groups[i] < 0) {
            ctx->groups[i] = gid;
            return true;
        }
    }

    return false;
}

/** Remove a group from a security context.
 * @param ctx           Context to remove from.
 * @param gid           Group ID to remove. */
static inline void security_context_remove_group(security_context_t *ctx, group_id_t gid) {
    size_t i;

    for (i = 0; i < SECURITY_CONTEXT_MAX_GROUPS; i++) {
        if (ctx->groups[i] == gid) {
            ctx->groups[i] = -1;
            return;
        }
    }
}

/** Check if a security context has a privilege.
 * @param ctx           Context to check in.
 * @param priv          Privilege to check for.
 * @return              Whether context has the privilege. */
static inline bool security_context_has_priv(const security_context_t *ctx, unsigned priv) {
    return (ctx->privs[priv / 32] & (1 << (priv % 32)));
}

/** Set a privilege in a security context.
 * @param ctx           Context to set in.
 * @param priv          Privilege to set. */
static inline void security_context_set_priv(security_context_t *ctx, unsigned priv) {
    ctx->privs[priv / 32] |= (1 << (priv % 32));
}

/** Remove a privilege from a security context.
 * @param ctx           Context to remove from.
 * @param priv          Privilege to remove. */
static inline void security_context_unset_priv(security_context_t *ctx, unsigned priv) {
    ctx->privs[priv / 32] &= ~(1 << (priv % 32));
}

/** Set a privilege in a security context.
 * @param ctx           Context to set in.
 * @param priv          Privilege to set. */
static inline void security_context_set_inherit(security_context_t *ctx, unsigned priv) {
    ctx->inherit[priv / 32] |= (1 << (priv % 32));
}

/** Remove a privilege from a security context.
 * @param context       Context to remove from.
 * @param priv          Privilege to remove. */
static inline void security_context_unset_inherit(security_context_t *ctx, unsigned priv) {
    ctx->inherit[priv / 32] &= ~(1 << (priv % 32));
}

extern status_t kern_token_create(const security_context_t *ctx, handle_t *_handle);
extern status_t kern_token_query(handle_t handle, security_context_t *ctx);

#ifdef __cplusplus
}
#endif
