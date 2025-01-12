/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Hello World test application.
 */

#include <core/time.h>

#include <kernel/thread.h>

#include <stdio.h>

int main(int argc, char **argv) {
    while (true) {
        printf("Hello, World!\n");
        kern_thread_sleep(core_secs_to_nsecs(1), NULL);
    }

    return 0;
}
