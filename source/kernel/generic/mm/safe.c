/* Kiwi safe user memory access functions
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
 * @brief		Safe user memory access functions.
 */

#include <arch/memmap.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/sched.h>
#include <proc/thread.h>

#include <errors.h>

/** Check if an address is valid. */
#if ASPACE_BASE == 0
# define VALID(addr, count)			\
	(((addr) + (count)) <= ASPACE_SIZE && ((addr) + (count)) >= (addr))
#else
# define VALID(addr, count)			\
	((addr) >= ASPACE_BASE && ((addr) + (count)) <= (ASPACE_BASE + ASPACE_SIZE) && ((addr) + (count)) >= (addr))
#endif

/** Common entry code for userspace memory functions. */
#define USERMEM_ENTER()				\
	if(context_save(&curr_thread->usermem_context) != 0) { \
		return -ERR_ADDR_INVAL; \
	} \
	atomic_set(&curr_thread->in_usermem, 1)

/** Common entry code for userspace memory functions. */
#define USERMEM_ENTER_CHECK(addr, count)	\
	if(!VALID((ptr_t)addr, count)) { \
		return -ERR_ADDR_INVAL; \
	} \
	USERMEM_ENTER()

/** Common exit code for userspace memory functions. */
#define USERMEM_EXIT()				\
	atomic_set(&curr_thread->in_usermem, 0)

/** Copy data from userspace.
 *
 * Copies data from a userspace source memory area to a kernel memory area.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		0 on success, -ERR_ADDR_INVAL on failure.
 */
int memcpy_from_user(void *dest, const void *src, size_t count) {
	USERMEM_ENTER_CHECK(src, count);

	memcpy(dest, src, count);

	USERMEM_EXIT();
	return 0;
}

/** Copy data to userspace.
 *
 * Copies data from a kernel memory area to a userspace memory area.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		0 on success, -ERR_ADDR_INVAL on failure.
 */
int memcpy_to_user(void *dest, const void *src, size_t count) {
	USERMEM_ENTER_CHECK(dest, count);

	memcpy(dest, src, count);

	USERMEM_EXIT();
	return 0;
}

/** Fill a userspace memory area.
 *
 * Fills a userspace memory area with the value specified.
 *
 * @param dest		The memory area to fill.
 * @param val		The value to fill with.
 * @param count		The number of bytes to fill.
 *
 * @return		0 on success, -ERR_ADDR_INVAL on failure.
 */
int memset_user(void *dest, int val, size_t count) {
	USERMEM_ENTER_CHECK(dest, count);

	memset(dest, val, count);

	USERMEM_EXIT();
	return 0;
}

/** Get length of userspace string.
 *
 * Gets the length of the specified string residing in a userspace memory
 * area. The length is the number of characters found before a NULL byte.
 *
 * @param str		Pointer to the string.
 * @param lenp		Where to store string length.
 * 
 * @return		0 on success, -ERR_ADDR_INVAL on failure.
 */
int strlen_user(const char *str, size_t *lenp) {
	size_t retval = 0;

	USERMEM_ENTER();

	/* Yeah... this is horrible. */
	while(true) {
		if(!VALID((ptr_t)str, retval + 1)) {
			USERMEM_EXIT();
			return -ERR_ADDR_INVAL;
		} else if(str[retval] == 0) {
			break;
		}

		retval++;
	}

	USERMEM_EXIT();
	*lenp = retval;
	return 0;
}

/** Copy a string from userspace.
 *
 * Copies a string from a userspace memory area to a kernel buffer. Assumes
 * that the destination is big enough to hold the string.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * 
 * @return		0 on success, -ERR_ADDR_INVAL on failure.
 */
int strcpy_from_user(char *dest, const char *src) {
	size_t i = 0;

	USERMEM_ENTER();

	/* Yeah... this is horrible. */
	while(1) {
		if(!VALID((ptr_t)src, i + 1)) {
			USERMEM_EXIT();
			return -ERR_ADDR_INVAL;
		} else if((*dest++ = src[i++]) == 0) {
			break;
		}
	}

	USERMEM_EXIT();
	return 0;
}

/** Duplicate string from userspace.
 *
 * Allocates a buffer big enough and copies across a string from userspace.
 *
 * @param src		Location to copy from.
 * @param mmflag	Allocation flags.
 * @param destp		Pointer to buffer in which to store destination.
 *
 * @return		0 on success, negative error code on failure.
 *			Returns -ERR_PARAM_INVAL if the string is zero-length.
 */
int strdup_from_user(const void *src, int mmflag, char **destp) {
	size_t len;
	char *d;
	int ret;

	ret = strlen_user(src, &len);
	if(ret != 0) {
		return ret;
	} else if(len == 0) {
		return -ERR_PARAM_INVAL;
	}

	d = kmalloc(len + 1, mmflag);
	if(d == NULL) {
		return -ERR_NO_MEMORY;
	}

	ret = memcpy_from_user(d, src, len);
	if(ret != 0) {
		kfree(d);
		return ret;
	}
	d[len] = 0;

	*destp = d;
	return 0;
}
