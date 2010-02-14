/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		RTLD argument functions.
 */

#include <kernel/errors.h>
#include <kernel/process.h>

#include "args.h"
#include "utility.h"

/** Define to 1 if it is desired for debug mode to always be enabled. */
#define RTLD_ALWAYS_DEBUG	0

/** Argument variables. */
bool rtld_debug = false;
bool rtld_dryrun = false;
char *rtld_extra_libpaths[16];

/** Parse an argument containing a list of paths.
 * @param str		String to parse.
 * @param arr		Array to store paths in.
 * @param max		Number of entries in array. */
static void rtld_args_parse_pathlist(const char *str, char **arr, size_t max) {
	size_t count = 0;
	char *dup, *tok;

	if(strlen(str) == 0) {
		return;
	} else if(!(dup = strdup(str))) {
		process_exit(ERR_NO_MEMORY);
	}

	while((tok = strsep(&dup, ":"))) {
		if(!tok[0]) {
			continue;
		} else if((count + 1) >= max) {
			return;
		}

		arr[count++] = tok;
		arr[count] = NULL;
	}
}

/** Parse arguments specified in the environment.
 * @param args		Arguments structure from kernel. */
void rtld_args_init(process_args_t *args) {
	char arg[32], *ptr;
	size_t len;
	int i;

	for(i = 0; i < args->env_count; i++) {
		if(!(ptr = strchr(args->env[i], '='))) {
			continue;
		} else if((len = ptr - args->env[i]) >= 32) {
			continue;
		}

		strncpy(arg, args->env[i], len);
		arg[len] = 0;

		if(strcmp(arg, "RTLD_DEBUG") == 0) {
			rtld_debug = true;
		} else if(strcmp(arg, "RTLD_DRYRUN") == 0) {
			rtld_dryrun = true;
		} else if(strcmp(arg, "RTLD_LIBPATH") == 0) {
			rtld_args_parse_pathlist(ptr + 1, rtld_extra_libpaths, 16);
		}
	}

#if RTLD_ALWAYS_DEBUG
	rtld_debug = true;
#endif
}
