/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Test application.
 */

#include <kernel/status.h>
#include <kernel/thread.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#define NUM_THREADS     8

static std::mutex test_lock;
static std::condition_variable test_cond;
static bool exiting;

static int thread_func(void *id) {
    std::unique_lock<std::mutex> lock(test_lock);

    if (reinterpret_cast<uintptr_t>(id) == 0) {
        while (!exiting) {
            lock.unlock();
            sleep(1);
            lock.lock();

            printf("Broadcasting\n");
            test_cond.notify_all();
        }
    } else {
        while (!exiting) {
            printf("Thread %zu waiting\n", reinterpret_cast<uintptr_t>(id));
            test_cond.wait(lock);
            printf("Thread %zu woken\n", reinterpret_cast<uintptr_t>(id));
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    status_t ret;

    std::cout << "Hello, World! My arguments are:" << std::endl;

    std::vector<std::string> args;
    args.assign(argv, argv + argc);

    unsigned long i = 0;
    for (const std::string &arg : args)
        std::cout << " args[" << i++ << "] = '" << arg << "'" << std::endl;

    printf("Acquiring lock...\n");
    test_lock.lock();

    printf("Creating threads...\n");

    object_event_t events[NUM_THREADS];
    for (i = 0; i < NUM_THREADS; i++) {
        ret = kern_thread_create(
            "test", thread_func, reinterpret_cast<void *>(i), nullptr, 0,
            &events[i].handle);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "Failed to create thread: %d\n", ret);
            return EXIT_FAILURE;
        }

        events[i].event = THREAD_EVENT_DEATH;

        thread_id_t id;
        kern_thread_id(events[i].handle, &id);

        printf("Created thread %" PRId32 ", handle %" PRId32 "\n", id, events[i].handle);
    }

    printf("Unlocking...\n");
    test_lock.unlock();

    sleep(20);

    test_lock.lock();
    printf("Exiting...\n");
    exiting = true;
    test_lock.unlock();

    ret = kern_object_wait(events, NUM_THREADS, OBJECT_WAIT_ALL, -1);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "Failed to wait for thread: %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("All threads exited\n");
    return 0;
}
