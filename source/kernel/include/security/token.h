/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Security tokens.
 */

#pragma once

#include <kernel/security.h>

#include <object.h>

/** Structure of a security token. */
typedef struct token {
    security_context_t ctx;         /**< Context that the token holds. */
    refcount_t count;               /**< Reference count for the token. */

    /**
     * Whether the token needs to be copied when inheriting.
     *
     * This is set when the effective and inheritable privilege sets differ, as
     * in this case the token must be copied when inheriting. If they are the
     * same, the same token can be shared.
     */
    bool copy_on_inherit;
} token_t;

extern token_t *system_token;

extern void token_retain(token_t *token);
extern void token_release(token_t *token);
extern token_t *token_inherit(token_t *source);

extern token_t *token_current(void);

extern status_t token_publish(token_t *token, handle_t *_id, handle_t *_uid);

extern void token_init(void);
