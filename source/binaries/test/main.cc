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

#include <iostream>

using namespace kiwi;
using namespace std;

static unsigned long foobar[2] = { 1337, 42 };

class MyClass : public Object {
public:
	MyClass() {
		cout << "Hello World! " << array_size(foobar) << endl;
	}
};

extern "C" void thread_set_tls_addr(void *addr);

int main(int argc, char **argv) {
	MyClass x;
	thread_set_tls_addr(foobar);
	unsigned long vala, valb;
#ifdef __x86_64__
	__asm__ volatile("movq %%fs:8, %0" : "=r"(vala));
	__asm__ volatile("movq %%fs:0, %0" : "=r"(valb));
#else
	__asm__ volatile("movl %%gs:4, %0" : "=r"(vala));
	__asm__ volatile("movl %%gs:0, %0" : "=r"(valb));
#endif
	cout << vala << endl;
	cout << valb << endl;
	while(1);
}
