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
 * @brief		Kernel object functions/definitions.
 */

#ifndef __KERNEL_OBJECT_H
#define __KERNEL_OBJECT_H

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Object type ID definitions. */
#define OBJECT_TYPE_FILE	1	/**< File. */
#define OBJECT_TYPE_DIR		2	/**< Directory. */
#define OBJECT_TYPE_DEVICE	3	/**< Device. */
#define OBJECT_TYPE_PROCESS	4	/**< Process. */
#define OBJECT_TYPE_THREAD	5	/**< Thread. */
#define OBJECT_TYPE_PORT	6	/**< IPC port. */
#define OBJECT_TYPE_CONNECTION	7	/**< IPC connection. */
#define OBJECT_TYPE_SEMAPHORE	8	/**< Semaphore. */
#define OBJECT_TYPE_AREA	9	/**< Memory area. */

/** ACL entry types. */
#define ACL_ENTRY_USER		0	/**< User (value of -1 means owning user). */
#define ACL_ENTRY_GROUP		1	/**< Group (value of -1 means owning group). */
#define ACL_ENTRY_OTHERS	2	/**< Others. */
#define ACL_ENTRY_SESSION	3	/**< Session. */
#define ACL_ENTRY_CAPABILITY	4	/**< Capability. */

/** Rights for all object types. */
#define OBJECT_READ_SECURITY	(1<<0)	/**< Read security information (ACL, owner). */
#define OBJECT_SET_OWNER	(1<<1)	/**< Set the object owner. */
#define OBJECT_SET_ACL		(1<<2)	/**< Set the access control list. */

/** Handle behaviour flags. */
#define HANDLE_INHERITABLE	(1<<0)	/**< Handle will be inherited by child processes. */

/** Details of an object event to wait for. */
typedef struct object_event {
	handle_t handle;		/**< Handle to wait on. */
	int event;			/**< Event to wait for. */
	bool signalled;			/**< Whether the event was signalled. */
} object_event_t;

/** Object ACL entry structure. */
typedef struct object_acl_entry {
	uint8_t type;			/**< Entry type. */
	int32_t value;			/**< Value specific to type (user ID, group ID). */
	uint32_t rights;		/**< Rights to grant. */
} object_acl_entry_t;

/** Object ACL structure. */
typedef struct object_acl {
	object_acl_entry_t *entries;	/**< Array of entries. */
	size_t count;			/**< Number of ACL entries. */
} object_acl_t;

extern int SYSCALL(object_type)(handle_t handle);
extern status_t SYSCALL(object_owner)(handle_t handle, user_id_t *uidp);
extern status_t SYSCALL(object_set_owner)(handle_t handle, user_id_t uid);
extern status_t SYSCALL(object_acl)(handle_t handle, object_acl_t *aclp);
extern status_t SYSCALL(object_set_acl)(handle_t handle, const object_acl_t *acl);
extern status_t SYSCALL(object_wait)(object_event_t *events, size_t count, useconds_t timeout);
extern status_t SYSCALL(handle_flags)(handle_t handle, int *flagsp);
extern status_t SYSCALL(handle_set_flags)(handle_t handle, int flags);
extern status_t SYSCALL(handle_duplicate)(handle_t handle, handle_t dest, bool force, handle_t *newp);
extern status_t SYSCALL(handle_close)(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_OBJECT_H */
