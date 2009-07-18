/* Kiwi system call number definitions
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
 * @brief		System call number definitions.
 */

#ifndef __KERNEL_SYSCALL_H
#define __KERNEL_SYSCALL_H

/** System call number definitions. */
#define __SYS_PUTCH			0
#define __SYS_HANDLE_CLOSE		1
#define __SYS_FS_FILE_CREATE		2
#define __SYS_FS_FILE_PATH		3
#define __SYS_FS_FILE_OPEN		4
#define __SYS_FS_FILE_READ		5
#define __SYS_FS_FILE_WRITE		6
#define __SYS_FS_FILE_TRUNCATE		7
#define __SYS_FS_FILE_SEEK		8
#define __SYS_FS_FILE_MAP		9
#define __SYS_FS_DIR_CREATE		10
#define __SYS_FS_DIR_OPEN		11
#define __SYS_FS_DIR_READ		12
#define __SYS_FS_SYMLINK_CREATE		13
#define __SYS_FS_SYMLINK_READ		14
#define __SYS_FS_INFO			15
#define __SYS_FS_LINK			16
#define __SYS_FS_UNLINK			17
#define __SYS_FS_RENAME			18
#define __SYS_FS_GETCWD			19
#define __SYS_FS_SETCWD			20
#define __SYS_FS_MOUNT			21
#define __SYS_FS_UNMOUNT		22

#endif /* __KERNEL_SYSCALL_H */
