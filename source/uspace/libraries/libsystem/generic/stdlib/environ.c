/* Environment variable functions
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
 * @brief		Environment variable functions.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../libsystem.h"

char **environ;

//static bool __environ_alloced = false;

/** Get the value of an environment variable.
 *
 * Gets the value of an environment variable stored in the environ array.
 * The string returned should not be modified.
 *
 * @param name		Name of variable to get.
 *
 * @return		Pointer to value.
 */
char *getenv(const char *name) {
	char *key, *val;
	size_t i, len;

	if(name == NULL) {
		return NULL;
	}

	for(i = 0; environ[i] != NULL; i++) {
		key = environ[i];
		val = strchr(key, '=');

		if(val == NULL) {
			__libsystem_fatal("Value '%s' found in environment without an = ***", key);
		}

		len = strlen(name);
		if(strncmp(key, name, len) == 0) {
			if(environ[i][len] == '=') {
				return val + 1;
			}
		}
	}

	return NULL;
}

#if 0
/** Set or change an environment variable.
 *
 * Sets or changes an environment variable. The variable will be set to the
 * given string, so changing it will change the environment. The string
 * should be in the form name=value.
 *
 * @param str		String to add.
 *
 * @return		0 on success, negative error code on failure.
 */
int putenv(char *str) {
	size_t count, len, tmp, i;
	char **new, *val;

	if(strchr(str, '=') == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* If not previously allocated, the environment is still on the stack
	 * so we cannot modify it. Duplicate it and point environ to the
	 * new location. */
	if(!__environ_alloced) {
		/* Get a count of what to copy. */
		for(count = 0; environ[count] != NULL; count++);

		new = malloc((count + 1) * sizeof(char *));
		if(new == NULL) {
			return -1;
		}

		memcpy(new, environ, (count + 1) * sizeof(char *));
		environ = new;
		__environ_alloced = true;
	}

	len = strchr(str, '=') - str;

	/* Check for an existing entry with the same name. */
	for(i = 0; environ[i] != NULL; i++) {
		tmp = strchr(environ[i], '=') - environ[i];

		if(len != tmp) {
			continue;
		} else if(strncmp(environ[i], str, len) == 0) {
			environ[i] = str;
			return 0;
		}
	}

	/* Doesn't exist at all. Reallocate environment to fit. */
	for(count = 0; environ[count] != NULL; count++);
	new = realloc(environ, (count + 2) * sizeof(char *));
	if(new == NULL) {
		free(val);
		return -1;
	}
	environ = new;

	/* Set new entry. */
	environ[count] = str;
	environ[count + 1] = NULL;

	return 0;
}

/** Set an environment variable.
 *
 * Sets an environment variable to the given value. The strings given will
 * be duplicated.
 *
 * @param name		Name of variable to set.
 * @param value		Value for variable.
 * @param overwrite	Whether to overwrite an existing value.
 *
 * @return		Pointer to value.
 */
int setenv(const char *name, const char *value, int overwrite) {
	char **new, *exist, *val;
	size_t count, len;

	if(strchr(name, '=') != NULL) {
		errno = EINVAL;
		return -1;
	}

	/* If not previously allocated, the environment is still on the stack
	 * so we cannot modify it. Duplicate it and point environ to the
	 * new location. */
	if(!__environ_alloced) {
		/* Get a count of what to copy. */
		for(count = 0; environ[count] != NULL; count++);

		new = malloc((count + 1) * sizeof(char *));
		if(new == NULL) {
			return -1;
		}

		memcpy(new, environ, (count + 1) * sizeof(char *));
		environ = new;
		__environ_alloced = true;
	}

	/* Work out total length. */
	len = strlen(name) + strlen(value) + 2;

	/* If it exists already, and the current value is big enough, just
	 * overwrite it. */
	if((exist = getenv(name))) {
		if(!overwrite) {
			return 0;
		}

		if(strlen(exist) >= strlen(value)) {
			strcpy(exist, value);
			return 0;
		}

		/* Find the entry in the environment array and reallocate
		 * it. */
		for(count = 0; environ[count] != NULL; count++) {
			if(strncmp(environ[count], name, strlen(name)) != 0) {
				continue;
			}

			val = malloc(len);
			if(val == NULL) {
				return -1;
			}

			sprintf(val, "%s=%s", name, value);
			environ[count] = val;
			return 0;
		}

		fprintf(stderr, "*** libc fatal: shouldn't get here in setenv! ***\n");
		abort();
	}

	/* Fill out the new entry. */
	val = malloc(len);
	if(val == NULL) {
		return -1;
	}
	sprintf(val, "%s=%s", name, value);

	/* Doesn't exist at all. Reallocate environment to fit. */
	for(count = 0; environ[count] != NULL; count++);
	new = realloc(environ, (count + 2) * sizeof(char *));
	if(new == NULL) {
		free(val);
		return -1;
	}
	environ = new;

	/* Set new entry. */
	environ[count] = val;
	environ[count + 1] = NULL;

	return 0;
}
#endif
