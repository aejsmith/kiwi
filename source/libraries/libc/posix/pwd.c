/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Password functions.
 *
 * @todo		Implement all of this.
 */

#include <pwd.h>
#include "../libc.h"

static struct passwd stub_pwd = {
        .pw_name = (char *)"admin",
        .pw_passwd = (char *)"meow",
       	.pw_uid = 0,
        .pw_gid = 0,
        .pw_dir = (char *)"/users/admin",
        .pw_shell = (char *)"/system/binaries/bash",
        .pw_gecos = (char *)"Administrator",
};

void endpwent(void) {
        return;
}

void setpwent(void) {
        return;
}

struct passwd *getpwuid(uid_t uid) {
        return &stub_pwd;
}
