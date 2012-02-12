/*
 * Copyright (C) 2009-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Safe user memory access functions.
 */

#include <arch/memory.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <setjmp.h>
#include <status.h>

/** Check if an address is valid. */
#if USER_BASE == 0
# define VALID(addr, count)		\
	(((addr) + (count)) <= USER_SIZE && ((addr) + (count)) >= (addr))
#else
# define VALID(addr, count)			\
	((addr) >= USER_BASE && ((addr) + (count)) <= (USER_BASE + USER_SIZE) && ((addr) + (count)) >= (addr))
#endif

/** Common entry code for userspace memory functions. */
#define USERMEM_ENTER()			\
	if(setjmp(curr_thread->usermem_context) != 0) { \
		return STATUS_INVALID_ADDR; \
	} \
	curr_thread->in_usermem = true

/** Common exit code for userspace memory functions. */
#define USERMEM_EXIT(status)		\
	curr_thread->in_usermem = false; \
	return (status)

/** Return success from a userspace memory function. */
#define USERMEM_SUCCESS()		USERMEM_EXIT(STATUS_SUCCESS)

/** Return failure from a userspace memory function. */
#define USERMEM_FAIL()			USERMEM_EXIT(STATUS_INVALID_ADDR)

/** Code to check parameters execute a statement. */
#define USERMEM_WRAP(addr, count, stmt)	\
	if(!VALID((ptr_t)addr, count)) { \
		return STATUS_INVALID_ADDR; \
	} \
	USERMEM_ENTER(); \
	stmt; \
	USERMEM_SUCCESS()

/** Check if an address range points with userspace memory.
 * @note		Does not check if the memory is actually accessible!
 * @param dest		Base address.
 * @param size		Size of range.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on failure. */
status_t validate_user_address(void *dest, size_t size) {
	return (VALID((ptr_t)dest, size)) ? STATUS_SUCCESS : STATUS_INVALID_ADDR;
}

/** Copy data from user memory.
 * @param dest		The kernel memory area to copy to.
 * @param src		The user memory area to copy from.
 * @param count		The number of bytes to copy.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure. */
status_t memcpy_from_user(void *dest, const void *src, size_t count) {
	USERMEM_WRAP(src, count, memcpy(dest, src, count));
}

/** Copy data to user memory.
 * @param dest		The user memory area to copy to.
 * @param src		The kernel memory area to copy from.
 * @param count		The number of bytes to copy.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure. */
status_t memcpy_to_user(void *dest, const void *src, size_t count) {
	USERMEM_WRAP(dest, count, memcpy(dest, src, count));
}

/** Fill a user memory area.
 * @param dest		The user memory area to fill.
 * @param val		The value to fill with.
 * @param count		The number of bytes to fill.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure. */
status_t memset_user(void *dest, int val, size_t count) {
	USERMEM_WRAP(dest, count, memset(dest, val, count));
}

/** Get the length of a user string.
 * @param str		Pointer to the string.
 * @param lenp		Where to store string length.
 * @return		STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *			failure. */
status_t strlen_user(const char *str, size_t *lenp) {
	size_t retval = 0;

	USERMEM_ENTER();

	/* Yeah... this is horrible. */
	while(true) {
		if(!VALID((ptr_t)str, retval + 1)) {
			USERMEM_FAIL();
		} else if(str[retval] == 0) {
			break;
		}

		retval++;
	}

	*lenp = retval;
	USERMEM_SUCCESS();
}

/**
 * Duplicate a string from user memory.
 *
 * Allocates a buffer large enough and copies across a string from user memory.
 * The allocation is not made using MM_WAIT, as there is no length limit and
 * therefore the length could be too large to fit in memory. Use of
 * strndup_from_user() is preferred to this.
 *
 * @param src		Location to copy from.
 * @param destp		Pointer to location in which to store address of
 *			destination buffer.
 *
 * @return		Status code describing result of the operation.
 *			Returns STATUS_INVALID_ARG if the string is
 *			zero-length.
 */
status_t strdup_from_user(const void *src, char **destp) {
	status_t ret;
	size_t len;
	char *d;

	ret = strlen_user(src, &len);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(len == 0) {
		return STATUS_INVALID_ARG;
	}

	d = kmalloc(len + 1, 0);
	if(!d) {
		return STATUS_NO_MEMORY;
	}

	ret = memcpy_from_user(d, src, len);
	if(ret != STATUS_SUCCESS) {
		kfree(d);
		return ret;
	}
	d[len] = 0;
	*destp = d;
	return STATUS_SUCCESS;
}

/**
 * Duplicate a string from user memory.
 *
 * Allocates a buffer large enough and copies across a string from user memory.
 * If the string is longer than the maximum length, then an error will be
 * returned. Because a length limit is provided, the allocation is made using
 * MM_WAIT - it is assumed that the limit is sensible.
 *
 * @param src		Location to copy from.
 * @param max		Maximum length allowed.
 * @param destp		Pointer to location in which to store address of
 *			destination buffer.
 *
 * @return		Status code describing result of the operation.
 *			Returns STATUS_INVALID_ARG if the string is
 *			zero-length.
 */
status_t strndup_from_user(const void *src, size_t max, char **destp) {
	status_t ret;
	size_t len;
	char *d;

	ret = strlen_user(src, &len);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(len == 0) {
		return STATUS_INVALID_ARG;
	} else if(len > max) {
		return STATUS_TOO_LONG;
	}

	d = kmalloc(len + 1, MM_WAIT);
	ret = memcpy_from_user(d, src, len);
	if(ret != STATUS_SUCCESS) {
		kfree(d);
		return ret;
	}
	d[len] = 0;
	*destp = d;
	return STATUS_SUCCESS;
}

/**
 * Copy a NULL-terminated array of strings from user memory.
 *
 * Copies a NULL-terminated array of strings from user memory. The array
 * itself and each array entry must be freed with kfree() once no longer
 * needed.
 *
 * @param src		Array to copy.
 * @param arrayp	Pointer to location in which to store address of
 *			allocated array.
 *
 * @return		Status code describing result of the operation.
 */
status_t arrcpy_from_user(const char *const src[], char ***arrayp) {
	char **array = NULL, **narr;
	status_t ret;
	int i;

	/* Copy the arrays across. */
	for(i = 0; ; i++) {
		narr = krealloc(array, sizeof(char *) * (i + 1), 0);
		if(!narr) {
			ret = STATUS_NO_MEMORY;
			goto fail;
		}

		array = narr;
		array[i] = NULL;

		ret = memcpy_from_user(&array[i], &src[i], sizeof(char *));
		if(ret != STATUS_SUCCESS) {
			array[i] = NULL;
			goto fail;
		} else if(array[i] == NULL) {
			break;
		}

		ret = strdup_from_user(array[i], &array[i]);
		if(ret != STATUS_SUCCESS) {
			array[i] = NULL;
			goto fail;
		}
	}

	*arrayp = array;
	return STATUS_SUCCESS;
fail:
	if(array) {
		for(i = 0; array[i] != NULL; i++) {
			kfree(array[i]);
		}
		kfree(array);
	}
	return ret;
}
