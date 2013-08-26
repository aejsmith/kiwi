/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Test application.
 */

#include <kernel/mutex.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#if 0
#define NUM_THREADS	8

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static bool exiting = false;

static void thread_func(void *id) {
	pthread_mutex_lock(&test_lock);

	if((unsigned long)id == 0) {
		while(!exiting) {
			pthread_mutex_unlock(&test_lock);
			sleep(1);
			pthread_mutex_lock(&test_lock);

			printf("Broadcasting\n");
			pthread_cond_broadcast(&test_cond);
		}
	} else {
		while(!exiting) {
			printf("Thread %u waiting\n", (unsigned long)id);
			pthread_cond_wait(&test_cond, &test_lock);
			printf("Thread %u woken\n", (unsigned long)id);
		}
	}

	pthread_mutex_unlock (&test_lock);
}
#endif

int main(int argc, char **argv) {
	std::cout << "Hello, World! My arguments are:" << std::endl;

	std::vector<std::string> args;
	args.assign(argv, argv + argc);

	int i = 0;
	for(const std::string &arg : args)
		std::cout << " args[" << i++ << "] = '" << arg << "'" << std::endl;

	try {
		throw std::runtime_error("Test exception");
	} catch(std::exception &e) {
		std::cout << "Caught exception '" << e.what() << "'" << std::endl;
		std::cout << typeid(e).name() << std::endl;
	}

	return 0;
#if 0
	int i;
	thread_entry_t entry;
	object_event_t events[NUM_THREADS];
	status_t ret;

	printf("Hello, World! My arguments are:\n");
	for(i = 0; i < argc; i++)
		printf(" argv[%d] = '%s'\n", i, argv[i]);

	printf("Acquiring lock...\n");
	pthread_mutex_lock(&test_lock);

	printf("Creating threads...\n");

	for(i = 0; i < NUM_THREADS; i++) {
		entry.func = thread_func;
		entry.arg = (void *)(unsigned long)i;
		entry.stack = NULL;
		entry.stack_size = 0;

		ret = kern_thread_create("test", &entry, 0, &events[i].handle);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to create thread: %d\n", ret);
			return EXIT_FAILURE;
		}

		events[i].event = THREAD_EVENT_DEATH;

		printf("Created thread %" PRId32 ", handle %" PRId32 "\n",
			kern_thread_id(events[i].handle), events[i].handle);
	}

	printf("Unlocking...\n");
	pthread_mutex_unlock(&test_lock);

	sleep(20);

	pthread_mutex_lock(&test_lock);
	printf("Exiting...\n");
	exiting = true;
	pthread_mutex_unlock(&test_lock);

	ret = kern_object_wait(events, NUM_THREADS, OBJECT_WAIT_ALL, -1);
	if(ret != STATUS_SUCCESS) {
		fprintf(stderr, "Failed to wait for thread: %d\n", ret);
		return EXIT_FAILURE;
	}

	printf("All threads exited\n");
	return 0;
#endif
}
