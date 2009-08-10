/* Kiwi x86 ELF helper functions
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
 * @brief		x86 ELF helper functions.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <proc/loader.h>

#include <elf.h>
#include <errors.h>

extern void elf_binary_copy_data(elf_binary_t *data);

/** Copy the data contained in a string array to the stack.
 * @param data		ELF binary data structure.
 * @param array		Array to copy data of.
 * @param count		Number of array entries.
 * @param dest		Array to store addresses copied to in. */
static void elf_binary_copy_array_data(elf_binary_t *data, char **array, size_t count, char **dest) {
	size_t i, len;

	for(i = 0; i < count; i++) {
		len = strlen(array[i]) + 1;

		/* Make room on the stack - keep aligned. */
		data->binary->stack -= ROUND_UP(len, sizeof(unative_t));
		dest[i] = (char *)data->binary->stack;

		memcpy(dest[i], array[i], len);
	}

	dest[count] = NULL;
}

/** Copy environment, arguments and auxilary data to the stack.
 * @param data		ELF binary data structure. */
void elf_binary_copy_data(elf_binary_t *data) {
	char **args, **environ;
	size_t argc, envc;

	/* Get the number of entries in the arrays. */
	for(argc = 0; data->binary->args[argc] != NULL; argc++);
	for(envc = 0; data->binary->environ[envc] != NULL; envc++);

	/* Allocate 2 temporary arrays to store the userspace addresses of the
	 * array data. */
	args = kcalloc(argc + 1, sizeof(char *), MM_SLEEP);
	environ = kcalloc(envc + 1, sizeof(char *), MM_SLEEP);

	/* Place the data contained in the arrays at the top of the stack. */
	elf_binary_copy_array_data(data, data->binary->environ, envc, environ);
	elf_binary_copy_array_data(data, data->binary->args, argc, args);

	/* TODO: Copy auxilary data here. */

	/* Copy the actual arrays to the stack. */
	data->binary->stack -= (envc + 1) * sizeof(char *);
	memcpy((void *)data->binary->stack, environ, (envc + 1) * sizeof(char *));
	data->binary->stack -= (argc + 1) * sizeof(char *);
	memcpy((void *)data->binary->stack, args, (argc + 1) * sizeof(char *));

	/* Finally write the argument count. */
	data->binary->stack -= sizeof(unative_t);
	*(int *)data->binary->stack = (int)argc;
}
