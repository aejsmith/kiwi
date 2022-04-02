/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Environment variable functions.
 *
 * TODO:
 *  - Use an rwlock.
 */

#include <core/mutex.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libsystem.h"

/** Pointer to the environment variable array. */
char **environ;

/** Whether the environment array has been allocated. */
static bool environ_alloced;

/** Environment lock. */
static CORE_MUTEX_DEFINE(environ_lock);

static bool ensure_environ_alloced(void) {
    /* If not previously allocated, the environment is still on the stack so we
     * cannot modify it. Duplicate it and point environ to the new location. */
    if (!environ_alloced) {
        /* Get a count of what to copy. */
        size_t count;
        for (count = 0; environ[count]; count++)
            ;

        char **new = malloc((count + 1) * sizeof(char *));
        if (!new)
            return false;

        for (size_t i = 0; i < count; i++) {
            new[i] = strdup(environ[i]);
            if (!new[i]) {
                for (size_t j = 0; j < i; j++)
                    free(new[i]);

                free(new);
                return false;
            }
        }

        new[count] = NULL;

        environ         = new;
        environ_alloced = true;
    }

    return true;
}

static char *getenv_unsafe(const char *name, size_t len, size_t *_index) {
    if (!environ)
        return NULL;

    for (size_t i = 0; environ[i]; i++) {
        char *key = environ[i];
        char *val = strchr(key, '=');

        if (!val)
            libsystem_fatal("value '%s' found in environment without an =", key);

        if (strncmp(key, name, len) == 0) {
            if (environ[i][len] == '=') {
                if (_index)
                    *_index = i;
                return val + 1;
            }
        }
    }

    return NULL;
}

/**
 * Gets the value of an environment variable stored in the environ array.
 * The string returned should not be modified.
 *
 * @param name          Name of variable to get.
 *
 * @return              Pointer to value.
 */
char *getenv(const char *name) {
    if (!name)
        return NULL;

    CORE_MUTEX_SCOPED_LOCK(lock, &environ_lock);
    return getenv_unsafe(name, strlen(name), NULL);
}

/**
 * Sets or changes an environment variable. The string should be in the form
 * name=value.
 *
 * @param str           String to add.
 *
 * @return              0 on success, -1 on failure.
 */
int putenv(char *str) {
    if (!str) {
        errno = EINVAL;
        return -1;
    }

    char *val       = strchr(str, '=');
    size_t name_len = val - str;
    if (!val || !name_len) {
        errno = EINVAL;
        return -1;
    }

    val++;

    /*
     * This function is specified to add the given string to the environment
     * rather than a copy so that modifying the string modifies the environment.
     * To me, this behaviour is completely broken. It also prevents us freeing
     * allocated environment variable strings.
     *
     * So, ignore the spec, and make a copy.
     */
    char *name __sys_cleanup_free = malloc(name_len + 1);
    if (!name)
        return -1;

    memcpy(name, str, name_len);
    name[name_len] = 0;

    return setenv(name, val, 1);
}

/**
 * Sets an environment variable to the given value. The strings given will
 * be duplicated.
 *
 * @param name          Name of variable to set.
 * @param value         Value for variable.
 * @param overwrite     Whether to overwrite an existing value.
 *
 * @return              Pointer to value.
 */
int setenv(const char *name, const char *value, int overwrite) {
    if (!name || name[0] == 0 || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    CORE_MUTEX_SCOPED_LOCK(lock, &environ_lock);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced())
        return -1;

    /* Work out total length. */
    size_t name_len  = strlen(name);
    size_t value_len = strlen(value);
    size_t len       = name_len + value_len + 2; /* = and terminator */

    /* If it exists already, and the current value is big enough, just
     * overwrite it. */
    size_t index;
    char *exist = getenv_unsafe(name, name_len, &index);
    if (exist) {
        if (!overwrite) {
            return 0;
        } else if (strlen(exist) == value_len) {
            strcpy(exist, value);
            return 0;
        }
    }

    /* Fill out the new entry. */
    char *str = malloc(len);
    if (!str)
        return -1;

    sprintf(str, "%s=%s", name, value);

    if (exist) {
        free(environ[index]);
        environ[index] = str;
    } else {
        /* Doesn't exist at all. Reallocate environment to fit. */
        size_t count;
        for (count = 0; environ[count]; count++)
            ;

        char **new = realloc(environ, (count + 2) * sizeof(char *));
        if (!new) {
            free(str);
            return -1;
        }

        environ = new;

        /* Set new entry. */
        environ[count] = str;
        environ[count + 1] = NULL;
    }

    return 0;
}

/** Unsets an environment variable.
 * @param name          Name of variable (must not contain an = character).
 * @return              0 on success, -1 on failure. */
int unsetenv(const char *name) {
    if (!name || !name[0] || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    CORE_MUTEX_SCOPED_LOCK(lock, &environ_lock);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced())
        return -1;

    size_t index;
    char *exist = getenv_unsafe(name, strlen(name), &index);
    if (exist) {
        free(environ[index]);

        while (environ[index]) {
            environ[index] = environ[index + 1];
            index++;
        }

        char **new = realloc(environ, index * sizeof(char *));
        if (new)
            environ = new;
    }

    return 0;
}
