#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/vm.h>

//#include <util/mutex.h>

#include <errno.h>
#include <stddef.h>
#include <time.h>

#include "libsystem.h"

#define LACKS_SYS_MMAN_H
#define LACKS_STDLIB_H

/* Available features */
#define HAVE_MMAP			1
#define HAVE_MREMAP			0
#define HAVE_MORECORE			0
#define NO_MALLINFO			1

/* Misc macro defines. */
#define ABORT				libsystem_fatal("dlmalloc abort");
#define USAGE_ERROR_ACTION(m, p)	\
	libsystem_fatal("dlmalloc usage error (%s:%d): %p, %p (ret: %p)\n", \
                   __FUNCTION__, __LINE__, m, p, __builtin_return_address(0));
#define MALLOC_FAILURE_ACTION		errno = ENOMEM;
// FIXME
#define malloc_getpagesize		((size_t)0x1000)

/** Wrapper for allocations. */
static inline void *mmap_wrapper(size_t size) {
	status_t ret;
	void *addr;

	ret = kern_vm_map(&addr, size, VM_ADDRESS_ANY, VM_PROT_READ | VM_PROT_WRITE,
		VM_MAP_PRIVATE, INVALID_HANDLE, 0, "dlmalloc");
	if(ret != STATUS_SUCCESS) {
		return (void *)-1;
	}

	return addr;
}

/** Wrapper for freeing. */
static inline int munmap_wrapper(void *start, size_t length) {
	return kern_vm_unmap(start, length);
}

/* To stop it defining dev_zero_fd. */
#define MAP_ANONYMOUS		0

#define MMAP(s)			mmap_wrapper((s))
#define DIRECT_MMAP(s)		mmap_wrapper((s))
#define MUNMAP(a, s)		munmap_wrapper((a), (s))

/* Locking. */
//#define USE_LOCKS 0

//#define MLOCK_T			libc_mutex_t
//#define INITIAL_LOCK(sl)	libc_mutex_init(sl)
//#define ACQUIRE_LOCK(sl)	libc_mutex_lock(sl, -1)
//#define RELEASE_LOCK(sl)	libc_mutex_unlock(sl)

//static MLOCK_T malloc_global_mutex = LIBC_MUTEX_INITIALISER;

#include "dlmalloc.c"
