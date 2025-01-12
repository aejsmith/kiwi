/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Security helper functions.
 */

#pragma once

#include <security/token.h>

/** Get the current security context.
 * @return              Pointer to current security context. */
static inline security_context_t *security_current_context(void) {
    token_t *token = token_current();

    return &token->ctx;
}

/** Get the current user ID.
 * @return              Current user ID. */
static inline user_id_t security_current_uid(void) {
    token_t *token = token_current();

    return token->ctx.uid;
}

/** Get the current group ID.
 * @return              Current group ID. */
static inline group_id_t security_current_gid(void) {
    token_t *token = token_current();

    return token->ctx.gid;
}

/** Check whether the current thread has a privilege.
 * @param priv          Privilege to check for.
 * @return              Whether the current thread has the privilege. */
static inline bool security_check_priv(unsigned priv) {
    token_t *token = token_current();

    return security_context_has_priv(&token->ctx, priv);
}
