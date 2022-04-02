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

#include "stdlib/environ.h"

#include "libsystem.h"

/** Pointer to the global environment variable array. */
char **environ;

/** Global environment wrapper. */
static environ_t global_environ = {
    .lock      = CORE_MUTEX_INITIALIZER,
    .array_ptr = &environ,
    .alloced   = false,
};

static bool ensure_environ_alloced(environ_t *env) {
    /* If not previously allocated, the environment is still on the stack so we
     * cannot modify it. Duplicate it and point environ to the new location. */
    if (!env->alloced) {
        char **array = *env->array_ptr;

        /* Get a count of what to copy. */
        size_t count;
        for (count = 0; array[count]; count++)
            ;

        char **new = malloc((count + 1) * sizeof(char *));
        if (!new)
            return false;

        for (size_t i = 0; i < count; i++) {
            new[i] = strdup(array[i]);
            if (!new[i]) {
                for (size_t j = 0; j < i; j++)
                    free(new[i]);

                free(new);
                return false;
            }
        }

        new[count] = NULL;

        *env->array_ptr = new;
        env->alloced    = true;
    }

    return true;
}

static char *environ_get_unsafe(environ_t *env, const char *name, size_t len, size_t *_index) {
    char **array = *env->array_ptr;

    if (!array)
        return NULL;

    for (size_t i = 0; array[i]; i++) {
        char *key = array[i];
        char *val = strchr(key, '=');

        if (!val)
            libsystem_fatal("value '%s' found in environment without an =", key);

        if (strncmp(key, name, len) == 0) {
            if (array[i][len] == '=') {
                if (_index)
                    *_index = i;
                return val + 1;
            }
        }
    }

    return NULL;
}

/** Gets the value of an environment variable.
 * @param env           Environment to get from.
 * @param name          Name of variable to get.
 * @return              Pointer to value. */
char *environ_get(environ_t *env, const char *name) {
    if (!name)
        return NULL;

    CORE_MUTEX_SCOPED_LOCK(lock, &env->lock);
    return environ_get_unsafe(env, name, strlen(name), NULL);
}

/**
 * Sets an environment variable to the given value. The strings given will
 * be duplicated.
 *
 * @param env           Environment to set in.
 * @param name          Name of variable to set.
 * @param value         Value for variable.
 * @param overwrite     Whether to overwrite an existing value.
 *
 * @return              Whether set successfully.
 */
bool environ_set(environ_t *env, const char *name, const char *value, bool overwrite) {
    if (!name || name[0] == 0 || strchr(name, '=')) {
        errno = EINVAL;
        return false;
    }

    CORE_MUTEX_SCOPED_LOCK(lock, &env->lock);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced(env))
        return false;

    /* Work out total length. */
    size_t name_len  = strlen(name);
    size_t value_len = strlen(value);
    size_t len       = name_len + value_len + 2; /* = and terminator */

    /* If it exists already, and the current value is big enough, just
     * overwrite it. */
    size_t index;
    char *exist = environ_get_unsafe(env, name, name_len, &index);
    if (exist) {
        if (!overwrite) {
            return true;
        } else if (strlen(exist) == value_len) {
            strcpy(exist, value);
            return true;
        }
    }

    /* Fill out the new entry. */
    char *str = malloc(len);
    if (!str)
        return false;

    sprintf(str, "%s=%s", name, value);

    char **array = *env->array_ptr;
    if (exist) {
        free(array[index]);
        array[index] = str;
    } else {
        /* Doesn't exist at all. Reallocate environment to fit. */
        size_t count;
        for (count = 0; array[count]; count++)
            ;

        char **new = realloc(array, (count + 2) * sizeof(char *));
        if (!new) {
            free(str);
            return false;
        }

        /* Set new entry. */
        new[count] = str;
        new[count + 1] = NULL;

        *env->array_ptr = new;
    }

    return true;
}

/** Unsets an environment variable.
 * @param env           Environment to unset in.
 * @param name          Name of variable (must not contain an = character).
 * @return              Whether unset successfully. */
bool environ_unset(environ_t *env, const char *name) {
    if (!name || !name[0] || strchr(name, '=')) {
        errno = EINVAL;
        return false;
    }

    CORE_MUTEX_SCOPED_LOCK(lock, &env->lock);

    /* Ensure the environment array can be modified. */
    if (!ensure_environ_alloced(env))
        return false;

    size_t index;
    char *exist = environ_get_unsafe(env, name, strlen(name), &index);
    if (exist) {
        char **array = *env->array_ptr;

        free(array[index]);

        while (array[index]) {
            array[index] = array[index + 1];
            index++;
        }

        char **new = realloc(array, index * sizeof(char *));
        if (new)
            *env->array_ptr = new;
    }

    return true;
}

/** Free the contents of the environment if it was allocated.
 * @param env           Environment to free. */
void environ_free(environ_t *env) {
    CORE_MUTEX_SCOPED_LOCK(lock, &env->lock);

    if (env->alloced) {
        char **array = *env->array_ptr;

        for (size_t i = 0; array[i]; i++)
            free(array[i]);

        free(array);
        *env->array_ptr = NULL;
    }
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
    return environ_get(&global_environ, name);
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
    return environ_set(&global_environ, name, value, overwrite) ? 0 : -1;
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

    return environ_set(&global_environ, name, val, true) ? 0 : -1;
}

/** Unsets an environment variable.
 * @param name          Name of variable (must not contain an = character).
 * @return              0 on success, -1 on failure. */
int unsetenv(const char *name) {
    return environ_unset(&global_environ, name) ? 0 : -1;
}
