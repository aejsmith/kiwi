/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Test application.
 */

#include <kiwi/Support/Utility.h>
#include <kiwi/Object.h>

#include <kernel/status.h>
#include <kernel/thread.h>

#include <iostream>

using namespace kiwi;
using namespace std;

extern __thread int test1;
__thread int test1 = 1234;
extern __thread int test2;

static unsigned long foobar[2] = { 1337, 42 };

class MyClass : public Object {
public:
	MyClass() {
		cout << "Hello World! " << array_size(foobar) << endl;
	}
};

static void thread_test(void *arg) {
	cout << endl << "Running test on thread " << thread_id(-1) << endl << endl;
	cout << test1 << endl;
	cout << test2 << endl;
	test2 = 32423 + thread_id(-1);
	cout << test2 << endl;
	{
		MyClass x;
	}
}

int main(int argc, char **argv) {
	thread_test(NULL);

	handle_t handle;
	status_t ret = thread_create("test", NULL, 0, thread_test, NULL, NULL, THREAD_QUERY, &handle);
	if(ret != STATUS_SUCCESS) {
		cout << "Failed to create thread: " << ret << endl;
	}
	object_event_t event = { handle, THREAD_EVENT_DEATH, false };
	object_wait(&event, 1, -1);
	int status;
	thread_status(handle, &status);
	cout << "Test thread exited with status " << status << endl;
	handle_close(handle);
	thread_test(NULL);
	while(1);
}
