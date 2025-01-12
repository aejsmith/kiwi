/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Password functions/definitions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** User information structure. */
struct passwd {
    char *pw_name;                  /**< User's login name. */
    char *pw_passwd;                /**< Password. */
    uid_t pw_uid;                   /**< Numerical user ID. */
    gid_t pw_gid;                   /**< Numerical group ID. */
    char *pw_dir;                   /**< Initial working directory. */
    char *pw_shell;                 /**< Program to use as shell. */
    char *pw_gecos;                 /**< Real name. */
};

extern void endpwent(void);
extern struct passwd *getpwent(void);
extern struct passwd *getpwnam(const char *name);
extern struct passwd *getpwuid(uid_t uid);
extern void setpwent(void);

__SYS_EXTERN_C_END
