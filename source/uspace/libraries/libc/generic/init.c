/* Kiwi C library - Initialization code.
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
 * @brief		Initialization code.
 */

#define __need_NULL
#include <stddef.h>

extern void __libc_init(int argc, char **argv, char **environ, void *auxv);
extern int main(int argc, char **argv, char **envp);
extern void __register_frame_info(void *begin, void *ob);
extern void *__deregister_frame_info(void *begin);

char **environ;

/** C library initialization function. */
void __libc_init(int argc, char **argv, char **envp, void *auxv) {
	environ = envp;

	main(argc, argv, envp);
	while(1);
}

void __register_frame_info(void *begin, void *ob) {
        return;
}

void *__deregister_frame_info(void *begin) {
        return NULL;
}
