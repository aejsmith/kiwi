/* Kiwi C++ test application
 * Copyright (C) 2009 Alex Smith
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
 * @brief		C++ test application.
 */

#include <kernel/device.h>

#include <kiwi/Process.h>

#include <cstdio>

using namespace kiwi;

class Hello {
public:
	Hello();
	void hello(void);
private:
	int foobar;
};

Hello::Hello() : foobar(1337) {
	printf("Constructing Hello!\n");
}

void Hello::hello(void) {
	printf("My value is %d\n", 1337);
}

Hello myhello;

extern "C" void putch(char ch);

int main(int argc, char **argv) {
	handle_t handle;
	int i, ret;
	char ch;

	printf("Hello! I'm process %d! My arguments are:\n", Process::get_current_id());
	for(i = 0; i < argc; i++) {
		printf(" argv[%d] = '%s'\n", i, argv[i]);
	}

	myhello.hello();

	handle = device_open("/input/keyboard");
	if(handle < 0) {
		printf("Device open failed: %d\n", handle);
		return 1;
	}

	while(true) {
		ret = device_read(handle, &ch, 1, 0, NULL);
		if(ret != 0) {
			printf("Read failed: %d\n", ret);
			return 1;
		}

		putch(ch);
	}
}
