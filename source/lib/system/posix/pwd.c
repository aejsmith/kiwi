/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Password functions.
 *
 * TODO:
 *  - Implement all of this.
 */

#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "libsystem.h"

static struct passwd stub_pwd = {
    .pw_name   = (char *)"admin",
    .pw_passwd = (char *)"meow",
    .pw_uid    = 0,
    .pw_gid    = 0,
    .pw_dir    = (char *)"/users/admin",
    .pw_shell  = (char *)"/system/bin/bash",
    .pw_gecos  = (char *)"Administrator",
};

static bool getpwent_called;

void endpwent(void) {
    getpwent_called = false;
    return;
}

struct passwd *getpwent(void) {
    if (!getpwent_called) {
        getpwent_called = true;
        return &stub_pwd;
    } else {
        return NULL;
    }
}

void setpwent(void) {
    return;
}

struct passwd *getpwnam(const char *name) {
    if (strcmp(name, stub_pwd.pw_name) == 0) {
        return &stub_pwd;
    } else {
        errno = ENOENT;
        return NULL;
    }
}

struct passwd *getpwuid(uid_t uid) {
    if (uid == stub_pwd.pw_uid) {
        return &stub_pwd;
    } else {
        errno = ENOENT;
        return NULL;
    }
}

char *getlogin(void) {
    return stub_pwd.pw_name;
}
