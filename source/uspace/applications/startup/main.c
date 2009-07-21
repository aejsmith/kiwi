/* Kiwi userspace startup application
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
 * @brief		Userspace startup application.
 */

#include <kernel/aspace.h>

#include <stdio.h>

int main(int argc, char **argv) {
	void *addr;
	int ret;

	printf("Hello from C userspace!\n");
	printf("This is a message!\n");

	ret = aspace_map_anon(NULL, 0x4000, ASPACE_MAP_READ | ASPACE_MAP_WRITE, &addr);
	printf("Map returned %d (%p)\n", ret, addr);
	if(ret == 0) {
		printf("Writing... 1234\n");
		*(int *)addr = 1234;
		printf("Reading... %d\n", *(int *)addr);
	}
	while(1);
}
