/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Dynamic linking functions.
 */

#include <dlfcn.h>

#include "libsystem.h"

/* TODO: Move dl_iterate_phdr here from libkernel. */

void *dlopen(const char *file, int mode) {
    libsystem_stub(__func__, false);
    return NULL;
}

int dlclose(void *handle) {
    libsystem_stub(__func__, false);
    return 0;
}

char *dlerror(void) {
    return (char *)"<UNIMPLEMENTED>";
}

void *dlsym(void *__restrict handle, const char *__restrict name) {
    libsystem_stub(__func__, false);
    return NULL;
}

int dladdr(void *addr, Dl_info *info) {
    //libsystem_stub(__func__, false);
    return 0;
}
