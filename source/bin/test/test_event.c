/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Event handling test application.
 */

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/time.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static volatile int remaining = 5;

static void timer_callback(object_event_t *event, thread_context_t *ctx) {
    printf("Callback (handle: %" PRId32 ", event: %u, udata: %p)\n", event->handle, event->event, event->udata);
    remaining--;
}

int main(int argc, char **argv) {
    handle_t timer;
    object_event_t event;
    status_t ret;

    ret = kern_timer_create(0, &timer);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create timer: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    event.handle = timer;
    event.event = TIMER_EVENT;
    event.flags = OBJECT_EVENT_EDGE;
    event.udata = (void *)0xdeadbeeful;
    ret = kern_object_callback(&event, timer_callback, 0);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to register callback: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    ret = kern_timer_start(timer, 1000000000, TIMER_PERIODIC);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to start timer: %" PRId32 "\n", ret);
        return EXIT_FAILURE;
    }

    while (remaining > 0)
        ;

    printf("Finished!\n");
    return EXIT_SUCCESS;
}
