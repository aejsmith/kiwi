/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Password functions.
 *
 * @todo		Implement all of this.
 */

#include <pwd.h>
#include <unistd.h>

#include "libsystem.h"

static struct passwd stub_pwd = {
        .pw_name = (char *)"admin",
        .pw_passwd = (char *)"meow",
       	.pw_uid = 0,
        .pw_gid = 0,
        .pw_dir = (char *)"/users/admin",
        .pw_shell = (char *)"/system/binaries/bash",
        .pw_gecos = (char *)"Administrator",
};

static bool getpwent_called = false;

void endpwent(void) {
	getpwent_called = false;
        return;
}

struct passwd *getpwent(void) {
	if(!getpwent_called) {
		getpwent_called = true;
		return &stub_pwd;
	} else {
		return NULL;
	}
}

void setpwent(void) {
        return;
}

struct passwd *getpwuid(uid_t uid) {
        return &stub_pwd;
}

char *getlogin(void) {
	return stub_pwd.pw_name;
}
