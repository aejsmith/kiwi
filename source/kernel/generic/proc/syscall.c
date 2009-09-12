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

#include <io/device.h>
#include <io/vfs.h>

#include <mm/malloc.h>
#include <mm/vm.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/syscall.h>

#include <sync/mutex.h>

#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <module.h>

/** Array of system call services. */
static syscall_service_t **syscall_services = NULL;
static size_t syscall_service_max = 0;
static MUTEX_DECLARE(syscall_services_lock, 0);

/** Print a character to the screen.
 * @param ch		Character to print.
 * @return		0 on success, negative error code on failure. */
static int sys_putch(char ch) {
	kprintf(LOG_NORMAL, "%c", ch);
	return 0;
}

/** Main kernel system call table. */
static syscall_handler_t kernel_syscall_table[] = {
	(syscall_handler_t)sys_putch,
	(syscall_handler_t)sys_module_load,
	(syscall_handler_t)sys_handle_close,
	(syscall_handler_t)sys_handle_type,
	(syscall_handler_t)sys_handle_wait,
	(syscall_handler_t)sys_handle_wait_multiple,
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
	(syscall_handler_t)sys_vm_map_anon,
	(syscall_handler_t)sys_vm_map_file,
	(syscall_handler_t)sys_vm_map_device,
	(syscall_handler_t)sys_vm_unmap,
	(syscall_handler_t)sys_process_create,
	(syscall_handler_t)sys_process_replace,
	(syscall_handler_t)sys_process_duplicate,
	(syscall_handler_t)sys_process_open,
	(syscall_handler_t)sys_process_id,
	(syscall_handler_t)sys_process_exit,
	(syscall_handler_t)sys_device_open,
	(syscall_handler_t)sys_device_read,
	(syscall_handler_t)sys_device_write,
	(syscall_handler_t)sys_device_request,
	(syscall_handler_t)sys_thread_create,
	(syscall_handler_t)sys_thread_open,
	(syscall_handler_t)sys_thread_id,
	(syscall_handler_t)sys_thread_exit,
};

/** Main kernel system call service. */
static syscall_service_t kernel_syscall_service = {
	.table = kernel_syscall_table,
	.size = ARRAYSZ(kernel_syscall_table),
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
	syscall_service_t *service;
	uint16_t num;

	num = (frame->id >> 16) & 0xFFFF;
	if(num > syscall_service_max) {
		return -ERR_SYSCALL_INVAL;
	}

	service = syscall_services[num];
	num = frame->id & 0xFFFF;
	if(!service || num >= service->size) {
		return -ERR_SYSCALL_INVAL;
	}

	return service->table[num](frame->p1, frame->p2, frame->p3, frame->p4, frame->p5, frame->p6);
}

/** Register a system call service.
 *
 * Registers a new system call service.
 *
 * @param num		Service number.
 * @param service	Service structure describing the service.
 *
 * @return		0 on success, negative error code on failure.
 */
int syscall_service_register(uint16_t num, syscall_service_t *service) {
	mutex_lock(&syscall_services_lock, 0);

	/* Resize the table if necessary. */
	if(num > syscall_service_max || !syscall_services) {
		syscall_services = krealloc(syscall_services, sizeof(ptr_t) * (num + 1), MM_SLEEP);
		memset(&syscall_services[syscall_service_max + 1], 0, (num - syscall_service_max) * sizeof(ptr_t *));

		syscall_service_max = num;
	} else if(syscall_services[num] != NULL) {
		mutex_unlock(&syscall_services_lock);
		return -ERR_ALREADY_EXISTS;
	}

	syscall_services[num] = service;
	kprintf(LOG_DEBUG, "syscall: registered system call service %" PRIu16 "(%p)\n", num, service);
	mutex_unlock(&syscall_services_lock);
	return 0;
}

/** Initialise the system call handling code. */
static void __init_text syscall_init(void) {
	if(syscall_service_register(0, &kernel_syscall_service) != 0) {
		fatal("Could not register kernel system call service");
	}
}
INITCALL(syscall_init);
