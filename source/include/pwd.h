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
 * @brief		Password functions/definitions.
 */

#ifndef __PWD_H
#define __PWD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** User information structure. */
struct passwd {
	char *pw_name;			/**< User's login name. */
	char *pw_passwd;		/**< Password. */
	uid_t pw_uid;			/**< Numerical user ID. */
	gid_t pw_gid;			/**< Numerical group ID. */
	char *pw_dir;			/**< Initial working directory. */
	char *pw_shell;			/**< Program to use as shell. */
	char *pw_gecos;			/**< Real name. */
};

extern void endpwent(void);
//extern struct passwd *getpwent(void);
//extern struct passwd *getpwnam(const char *name);
extern struct passwd *getpwuid(uid_t uid);
extern void setpwent(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWD_H */
