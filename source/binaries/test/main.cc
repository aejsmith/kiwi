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

#include <kiwi/Support/Mutex.h>
#include <kiwi/Support/Utility.h>
#include <kiwi/Process.h>
#include <kiwi/Thread.h>

#include <iostream>

using namespace kiwi;
using namespace std;

static Mutex test_mutex;

class TestThread : public Thread {
private:
	int Main() {
		test_mutex.Acquire();
		cout << "Test thread (" << GetCurrentID() << ") running!" << endl;
		Sleep(1000000);
		cout << "Test thread releasing lock" << endl;
		test_mutex.Release();
		Sleep(500000);
		test_mutex.Acquire();
		cout << "Test thread got lock again" << endl;
		test_mutex.Release();

		return Thread::Main();
	}
};

int main(int argc, char **argv) {
	if(Process::GetCurrentID() > 1) {
		cout << "I'm the child!" << endl;
		Thread::Sleep(1000000);
		return 42;
	}

	TestThread thread;
	thread.SetName("test_thread");
	if(!thread.Run()) {
		cout << "Failed to start test thread: " << thread.GetError().GetDescription() << endl;
		return 1;
	}

	Thread::Sleep(500000);
	test_mutex.Acquire();
	cout << "Main thread (" << Thread::GetCurrentID() << ") got lock" << endl;
	Thread::Sleep(1000000);
	cout << "Main thread releasing lock" << endl;
	test_mutex.Release();

	thread.Wait();
	cout << "Thread exited with status " << thread.GetStatus() << endl;

	Process child;
	if(!child.Create("test")) {
		cout << "Failed to start child process: " << child.GetError().GetDescription() << endl;
		return 1;
	}
	child.Wait();
	cout << "Child exited with status " << child.GetStatus() << endl;
	while(1);
}
