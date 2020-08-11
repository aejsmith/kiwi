/*
 * Copyright (C) 2009-2020 Alex Smith
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

/** Reallocate the contents of the environment if necessary.
 * @return              Whether succesful. */
static bool ensure_environ_alloced(void) {
    size_t count;
    char **new;

    /* If not previously allocated, the environment is still on the stack so we
     * cannot modify it. Duplicate it and point environ to the new location. */
    if (!environ_alloced) {
        /* Get a count of what to copy. */
        for (count = 0; environ[count]; count++)
            ;

        new = malloc((count + 1) * sizeof(char *));
        if (!new)
            return false;

        memcpy(new, environ, (count + 1) * sizeof(char *));
        environ = new;
        environ_alloced = true;
    }

    return true;
}

/** Get the value of an environment variable without taking lock.
 * @param name          Name of variable to get.
 * @return              Pointer to value. */
static char *getenv_unsafe(const char *name) {
    char *key, *val;
    size_t i, len;

    for (i = 0; environ[i]; i++) {
        key = environ[i];
        val = strchr(key, '=');

        if (!val)
            libsystem_fatal("value '%s' found in environment without an =", key);

        len = strlen(name);
        if (strncmp(key, name, len) == 0) {
            if (environ[i][len] == '=')
                return val + 1;
        }
    }

    return NULL;
}

/**
 * Get the value of an environment variable.
 *
 * Gets the value of an environment variable stored in the environ array.
 * The string returned should not be modified.
 *
 * @param name          Name of variable to get.
 *
 * @return              Pointer to value.
 */
char *getenv(const char *name) {
    char *ret;

    if (!name)
        return NULL;

    core_mutex_lock(&environ_lock, -1);
    ret = getenv_unsafe(name);
    core_mutex_unlock(&environ_lock);

    return ret;
}

/**
 * Set or change an environment variable.
 *
 * Sets or changes an environment variable. The variable will be set to the
 * given string, so changing it will change the environment. The string
 * should be in the form name=value.
 *
 * @param str           String to add.
 *
 * @return              0 on success, -1 on failure.
 */
int putenv(char *str) {
    size_t count, len, tmp, i;
    char **new;

    if (!str || !strchr(str, '=')) {
        errno = EINVAL;
        return -1;
    } else if ((len = strchr(str, '=') - str) == 0) {
        errno = EINVAL;
        return -1;
    }

    core_mutex_lock(&environ_lock, -1);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced()) {
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    /* Check for an existing entry with the same name. */
    for (i = 0; environ[i]; i++) {
        tmp = strchr(environ[i], '=') - environ[i];

        if (len != tmp) {
            continue;
        } else if (strncmp(environ[i], str, len) == 0) {
            environ[i] = str;

            core_mutex_unlock(&environ_lock);
            return 0;
        }
    }

    /* Doesn't exist at all. Reallocate environment to fit. */
    for (count = 0; environ[count]; count++);
    new = realloc(environ, (count + 2) * sizeof(char *));
    if (!new) {
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    environ = new;

    /* Set new entry. */
    environ[count] = str;
    environ[count + 1] = NULL;

    core_mutex_unlock(&environ_lock);
    return 0;
}

/**
 * Set an environment variable.
 *
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
    char **new, *exist, *val;
    size_t count, len;

    if (!name || name[0] == 0 || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    core_mutex_lock(&environ_lock, -1);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced()) {
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    /* Work out total length. */
    len = strlen(name) + strlen(value) + 2;

    /* If it exists already, and the current value is big enough, just
     * overwrite it. */
    exist = getenv_unsafe(name);
    if (exist) {
        if (!overwrite) {
            core_mutex_unlock(&environ_lock);
            return 0;
        }

        if (strlen(exist) >= strlen(value)) {
            strcpy(exist, value);

            core_mutex_unlock(&environ_lock);
            return 0;
        }

        /* Find the entry in the environment array and reallocate it. */
        for (count = 0; environ[count]; count++) {
            if (strncmp(environ[count], name, strlen(name)) != 0)
                continue;

            val = malloc(len);
            if (!val) {
                core_mutex_unlock(&environ_lock);
                return -1;
            }

            sprintf(val, "%s=%s", name, value);
            environ[count] = val;

            core_mutex_unlock(&environ_lock);
            return 0;
        }

        libsystem_fatal("shouldn't get here in setenv");
    }

    /* Fill out the new entry. */
    val = malloc(len);
    if (!val) {
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    sprintf(val, "%s=%s", name, value);

    /* Doesn't exist at all. Reallocate environment to fit. */
    for (count = 0; environ[count]; count++)
        ;

    new = realloc(environ, (count + 2) * sizeof(char *));
    if (!new) {
        free(val);
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    environ = new;

    /* Set new entry. */
    environ[count] = val;
    environ[count + 1] = NULL;

    core_mutex_unlock(&environ_lock);
    return 0;
}

/** Unset an environment variable.
 * @param name          Name of variable (must not contain an = character).
 * @return              0 on success, -1 on failure. */
int unsetenv(const char *name) {
    char **new, *key, *val;
    size_t i, len, count;

    if (!name || !name[0] || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    core_mutex_lock(&environ_lock, -1);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced()) {
        core_mutex_unlock(&environ_lock);
        return -1;
    }

    for (i = 0; environ[i]; i++) {
        key = environ[i];
        val = strchr(key, '=');

        if (!val)
            libsystem_fatal("value '%s' found in environment without an =", key);

        len = strlen(name);
        if (strncmp(key, name, len) == 0 && environ[i][len] == '=') {
            for (count = 0; environ[count]; count++)
                ;

            memcpy(&environ[i], &environ[i + 1], (count - i) * sizeof(char *));
            new = realloc(environ, count * sizeof(char *));
            if (new)
                environ = new;

            core_mutex_unlock(&environ_lock);
            return 0;
        }
    }

    core_mutex_unlock(&environ_lock);
    return 0;
}
