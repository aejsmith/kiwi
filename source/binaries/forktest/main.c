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
 * @brief		Fork test app.
 */

#include <kernel/thread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	char *str;
	pid_t ret;

	str = malloc(32);
	strcpy(str, "Hello, World!");

	ret = fork();
	if(ret > 0) {
		thread_usleep(100000);
		printf("I'm the parent, child is %d. Memory contains: %s\n", ret, str);
	} else if(ret == 0) {
		printf("I'm the child, contains: %s\n", str);
		strcpy(str, "Meow meow meow meow!");
		printf("Now contains: %s\n", str);
	} else {
		perror("fork");
		return 1;
	}
	return 0;
}
