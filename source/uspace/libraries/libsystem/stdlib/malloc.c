#include <kernel/vm.h>

#include <stddef.h>

#include "../libsystem.h"

#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_SYS_TYPES_H
#define LACKS_FCNTL_H
#define LACKS_ERRNO_H
#define LACKS_UNISTD_H
#define LACKS_STDLIB_H

/* Available features */
#define HAVE_MMAP			1
#define HAVE_MREMAP			0
#define HAVE_MORECORE			0
#define NO_MALLINFO			1

/* Misc macro defines. */
#define ABORT				__libsystem_fatal("dlmalloc abort");
#define USAGE_ERROR_ACTION(m, p)	\
	__libsystem_fatal("dlmalloc usage error (%s:%d): %p, %p (ret: %p)\n", \
                          __FUNCTION__, __LINE__, m, p, __builtin_return_address(0));
#define MALLOC_FAILURE_ACTION
#define malloc_getpagesize		((size_t)0x1000)

/** Temporary. */
#define time(p)				1248184472

/** Wrapper for allocations. */
static inline void *mmap_wrapper(size_t size) {
	void *addr;
	int ret;

	ret = vm_map(NULL, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, -1, 0, &addr);
	if(ret != 0) {
		return (void *)-1;
	}

	return addr;
}

/** Wrapper for freeing. */
static inline int munmap_wrapper(void *start, size_t length) {
	return vm_unmap(start, length);
}

/* To stop it defining dev_zero_fd. */
#define MAP_ANONYMOUS		0

#define MMAP(s)			mmap_wrapper((s))
#define DIRECT_MMAP(s)		mmap_wrapper((s))
#define MUNMAP(a, s)		munmap_wrapper((a), (s))

#include "dlmalloc.c"
