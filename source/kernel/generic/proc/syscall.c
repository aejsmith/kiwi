/* Kiwi system call dispatcher
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
 * @brief		System call dispatcher.
 */

#include <arch/syscall.h>

#include <console/kprintf.h>

#include <io/vfs.h>

#include <mm/vm.h>

#include <lib/utility.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/syscall.h>

#include <errors.h>
#include <fatal.h>
#include <module.h>

/** Print a character to the screen.
 * @param ch		Character to print.
 * @return		0 on success, negative error code on failure. */
static int sys_putch(char ch) {
	kprintf(LOG_NORMAL, "%c", ch);
	return 0;
}

/** Table of system calls. */
static syscall_handler_t syscall_table[] = {
	(syscall_handler_t)sys_putch,

	/** Module loader system calls. */
	(syscall_handler_t)sys_module_load,

	/** Handle system calls. */
	(syscall_handler_t)sys_handle_close,
	(syscall_handler_t)sys_handle_type,

	/** Filesystem system calls. */
	(syscall_handler_t)sys_fs_file_create,
	(syscall_handler_t)sys_fs_file_open,
	(syscall_handler_t)sys_fs_file_read,
	(syscall_handler_t)sys_fs_file_write,
	(syscall_handler_t)sys_fs_file_resize,
	(syscall_handler_t)sys_fs_dir_create,
	(syscall_handler_t)sys_fs_dir_open,
	(syscall_handler_t)sys_fs_dir_read,
	(syscall_handler_t)sys_fs_handle_seek,
	(syscall_handler_t)sys_fs_handle_info,
	(syscall_handler_t)sys_fs_symlink_create,
	(syscall_handler_t)sys_fs_symlink_read,
	(syscall_handler_t)sys_fs_mount,
	(syscall_handler_t)sys_fs_unmount,
	(syscall_handler_t)sys_fs_getcwd,
	(syscall_handler_t)sys_fs_setcwd,
	(syscall_handler_t)sys_fs_setroot,
	(syscall_handler_t)sys_fs_info,
	(syscall_handler_t)sys_fs_link,
	(syscall_handler_t)sys_fs_unlink,
	(syscall_handler_t)sys_fs_rename,

	/** VM system calls. */
	(syscall_handler_t)sys_vm_map_anon,
	(syscall_handler_t)sys_vm_map_file,
	(syscall_handler_t)sys_vm_unmap,
};

/** System call dispatcher.
 *
 * Handles a system call from a userspace process. It simply forwards the call
 * to the function defined by the process' subsystem.
 *
 * @param frame		System call frame structure.
 *
 * @return		Return value of the system call.
 */
unative_t syscall_handler(syscall_frame_t *frame) {
	if(frame->id >= ARRAYSZ(syscall_table)) {
		/* TODO: Kill the process. */
		fatal("Invalid system call. TODO: Kill process");
	}

	return syscall_table[frame->id](frame->p1, frame->p2, frame->p3,
	                                frame->p4, frame->p5, frame->p6);
}
