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

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/time.h>

#include <stdio.h>
#include <stdlib.h>

/** Convert seconds to nanoseconds. */
#define SECS2NSECS(secs)	((nstime_t)secs * 1000000000)

static nstime_t times[5] = {
	SECS2NSECS(5), SECS2NSECS(1), SECS2NSECS(3), SECS2NSECS(4), SECS2NSECS(2)
};

int main(int argc, char **argv) {
	int i;
	object_event_t events[5];
	status_t ret;

	printf("Hello, World! My arguments are:\n");
	for(i = 0; i < argc; i++)
		printf(" argv[%d] = '%s'\n", i, argv[i]);

	for(i = 0; i < 5; i++) {
		ret = kern_timer_create(0, &events[i].handle);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to create timer: %d\n", ret);
			return EXIT_FAILURE;
		}

		events[i].event = TIMER_EVENT_FIRED;

		ret = kern_timer_start(events[i].handle, times[i], TIMER_ONESHOT);
		if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to start timer: %d\n", ret);
			return EXIT_FAILURE;
		}

		printf("Created timer %d (%d) for %lld nanoseconds\n", i,
			events[i].handle, times[i]);
	}

	while(true) {
		ret = kern_object_wait(events, 5, OBJECT_WAIT_ALL, 0);
		if(ret == STATUS_TIMED_OUT) {
			printf("Timed out\n");
		} else if(ret != STATUS_SUCCESS) {
			fprintf(stderr, "Failed to wait for events: %d\n", ret);
			return EXIT_FAILURE;
		}

		printf("Events fired:\n");
		for(i = 0; i < 5; i++) {
			if(events[i].signalled)
				printf("Timer %d (%d)\n", i, events[i].handle);
		}
	}
}
