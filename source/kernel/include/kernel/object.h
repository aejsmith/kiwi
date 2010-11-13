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

#include <kernel/types.h>

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
#define OBJECT_TYPE_TIMER	10	/**< Timer. */

/** Maximum ACL size. */
#define OBJECT_ACL_MAX		64

/** ACL entry types. */
#define ACL_ENTRY_USER		0	/**< User (value of -1 means owning user). */
#define ACL_ENTRY_GROUP		1	/**< Group (value of -1 means owning group). */
#define ACL_ENTRY_OTHERS	2	/**< Others. */
#define ACL_ENTRY_SESSION	3	/**< Session. */
#define ACL_ENTRY_CAPABILITY	4	/**< Capability. */

/** Rights for all object types. */
#define OBJECT_SET_ACL		(1<<0)	/**< Set the access control list. */
#define OBJECT_SET_OWNER	(1<<1)	/**< Set the owner of the object. */

/** Handle behaviour flags. */
#define HANDLE_INHERITABLE	(1<<0)	/**< Handle will be inherited by child processes. */

/** Type used to store a set of object rights. */
typedef uint32_t object_rights_t;

/** Details of an object event to wait for. */
typedef struct object_event {
	handle_t handle;		/**< Handle to wait on. */
	int event;			/**< Event to wait for. */
	bool signalled;			/**< Whether the event was signalled. */
} object_event_t;

/** Object ACL entry structure. */
typedef struct object_acl_entry {
	uint8_t type;			/**< Entry type. */
	int32_t value;			/**< Value specific to type (user/group/session ID, capability). */
	object_rights_t rights;		/**< Rights to grant. */
} object_acl_entry_t;

/** Object ACL structure. */
typedef struct object_acl {
	object_acl_entry_t *entries;	/**< Array of entries. */
	size_t count;			/**< Number of ACL entries. */
} object_acl_t;

/** Object security information structure. */
typedef struct object_security {
	user_id_t uid;			/**< Owning user ID (-1 means use current UID). */
	group_id_t gid;			/**< Owning group ID (-1 means use current GID). */
	object_acl_t *acl;		/**< Access control list (if NULL, default will be used). */
} object_security_t;

extern int SYSCALL(object_type)(handle_t handle);
extern status_t SYSCALL(object_set_security)(handle_t handle, const object_security_t *security);
extern status_t SYSCALL(object_wait)(object_event_t *events, size_t count, useconds_t timeout);

extern status_t SYSCALL(handle_flags)(handle_t handle, int *flagsp);
extern status_t SYSCALL(handle_set_flags)(handle_t handle, int flags);
extern status_t SYSCALL(handle_duplicate)(handle_t handle, handle_t dest, bool force, handle_t *newp);
extern status_t SYSCALL(handle_close)(handle_t handle);

#ifndef KERNEL
extern status_t object_security(handle_t handle, object_security_t *securityp);
extern object_acl_t *object_security_acl(object_security_t *security);
extern void object_security_destroy(object_security_t *security);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_OBJECT_H */
