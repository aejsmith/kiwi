/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <proc/process.h>
#include <proc/syscall.h>
#include <proc/thread.h>

#include <public/device.h>
#include <public/fs.h>
#include <public/object.h>
#include <public/vm.h>

#include <sync/rwlock.h>

#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>
#include <symbol.h>
#include <time.h>

/** Array of system call services. */
static syscall_service_t **syscall_services = NULL;
static size_t syscall_service_max = 0;
static RWLOCK_DECLARE(syscall_services_lock);

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
	(syscall_handler_t)sys_object_type,
	(syscall_handler_t)sys_object_wait,
	(syscall_handler_t)sys_object_wait_multiple,
	(syscall_handler_t)sys_handle_get_flags,
	(syscall_handler_t)sys_handle_set_flags,
	(syscall_handler_t)sys_handle_close,
	(syscall_handler_t)sys_fs_file_create,
	(syscall_handler_t)sys_fs_file_open,
	(syscall_handler_t)sys_fs_file_read,
	(syscall_handler_t)sys_fs_file_pread,
	(syscall_handler_t)sys_fs_file_write,
	(syscall_handler_t)sys_fs_file_pwrite,
	(syscall_handler_t)sys_fs_file_resize,
	(syscall_handler_t)sys_fs_dir_create,
	(syscall_handler_t)sys_fs_dir_open,
	(syscall_handler_t)sys_fs_dir_read,
	(syscall_handler_t)sys_fs_handle_seek,
	(syscall_handler_t)sys_fs_handle_info,
	(syscall_handler_t)sys_fs_handle_sync,
	(syscall_handler_t)sys_fs_symlink_create,
	(syscall_handler_t)sys_fs_symlink_read,
	(syscall_handler_t)sys_fs_mount,
	(syscall_handler_t)sys_fs_unmount,
	(syscall_handler_t)sys_fs_sync,
	(syscall_handler_t)sys_fs_getcwd,
	(syscall_handler_t)sys_fs_setcwd,
	(syscall_handler_t)sys_fs_setroot,
	(syscall_handler_t)sys_fs_info,
	(syscall_handler_t)sys_fs_link,
	(syscall_handler_t)sys_fs_unlink,
	(syscall_handler_t)sys_fs_rename,
	(syscall_handler_t)sys_vm_map,
	(syscall_handler_t)sys_vm_unmap,
	(syscall_handler_t)sys_process_create,
	(syscall_handler_t)sys_process_replace,
	(syscall_handler_t)sys_process_clone,
	(syscall_handler_t)sys_process_open,
	(syscall_handler_t)sys_process_id,
	(syscall_handler_t)sys_process_status,
	(syscall_handler_t)sys_process_exit,
	(syscall_handler_t)sys_device_open,
	(syscall_handler_t)sys_device_read,
	(syscall_handler_t)sys_device_write,
	(syscall_handler_t)sys_device_request,
	(syscall_handler_t)sys_thread_create,
	(syscall_handler_t)sys_thread_open,
	(syscall_handler_t)sys_thread_id,
	(syscall_handler_t)sys_thread_exit,
	(syscall_handler_t)sys_thread_usleep,
#if 0
	(syscall_handler_t)sys_ipc_port_create,
	(syscall_handler_t)sys_ipc_port_open,
	(syscall_handler_t)sys_ipc_port_id,
	(syscall_handler_t)sys_ipc_port_listen,
	(syscall_handler_t)sys_ipc_port_acl_add,
	(syscall_handler_t)sys_ipc_port_acl_remove,
	(syscall_handler_t)sys_ipc_connection_open,
	(syscall_handler_t)sys_ipc_message_send,
	(syscall_handler_t)sys_ipc_message_sendv,
	(syscall_handler_t)sys_ipc_message_receive,
#endif
};

/** Main kernel system call service. */
static syscall_service_t kernel_syscall_service = {
	.table = kernel_syscall_table,
	.size = ARRAYSZ(kernel_syscall_table),
};

/** System call dispatcher.
 * @param frame		System call frame structure.
 * @return		Return value of the system call. */
unative_t syscall_handler(syscall_frame_t *frame) {
	syscall_service_t *service;
#if CONFIG_TRACE_SYSCALLS
	useconds_t start, end;
	symbol_t *symbol;
	unative_t ret;
#endif
	uint16_t num;

	/* Kill the thread now if required - if it's been killed there's no
	 * need to bother carrying out the system call. */
	if(curr_thread->killed) {
		thread_exit();
	}

	/* Get the service number. */
	num = (frame->id >> 16) & 0xFFFF;
	if(num == 0) {
		/* The kernel service is always installed - it is given special
		 * handling so that it is not necessary to lock the service
		 * array lock for kernel system calls. */
		service = &kernel_syscall_service;
	} else {
		rwlock_read_lock(&syscall_services_lock);
		if(num > syscall_service_max || !(service = syscall_services[num])) {
			rwlock_unlock(&syscall_services_lock);
			return -ERR_SYSCALL_INVAL;
		}
		rwlock_unlock(&syscall_services_lock);
	}

	/* Get the call number. */
	num = frame->id & 0xFFFF;
	if(num >= service->size) {
		return -ERR_SYSCALL_INVAL;
	}

#if CONFIG_TRACE_SYSCALLS
	start = time_since_boot();
	ret = service->table[num](frame->p1, frame->p2, frame->p3, frame->p4, frame->p5, frame->p6);
	end = time_since_boot();

	symbol = symbol_lookup_addr((ptr_t)service->table[num], NULL);
	kprintf(LOG_DEBUG, "[%" PRIu32 "(%s):%" PRIu32 "(%s)] %s(0x%lx, 0x%lx, 0x%lx, "
	                   "0x%lx, 0x%lx, 0x%lx) = 0x%lx [%llu microseconds]\n",
	        curr_proc->id, curr_proc->name, curr_thread->id, curr_thread->name,
	        (symbol) ? symbol->name : "<unknown>", frame->p1, frame->p2,
	        frame->p3, frame->p4, frame->p5, frame->p6, ret, end - start);
	return ret;
#else
	return service->table[num](frame->p1, frame->p2, frame->p3, frame->p4, frame->p5, frame->p6);
#endif
}

/** Register a system call service.
 * @param num		Service number.
 * @param service	Service structure describing the service.
 * @return		0 on success, negative error code on failure. */
int syscall_service_register(uint16_t num, syscall_service_t *service) {
	rwlock_write_lock(&syscall_services_lock);

	/* Resize the table if necessary. */
	if(num > syscall_service_max || !syscall_services) {
		syscall_services = krealloc(syscall_services, sizeof(ptr_t) * (num + 1), MM_SLEEP);
		memset(&syscall_services[syscall_service_max + 1], 0, (num - syscall_service_max) * sizeof(ptr_t *));

		syscall_service_max = num;
	} else if(syscall_services[num] != NULL) {
		rwlock_unlock(&syscall_services_lock);
		return -ERR_ALREADY_EXISTS;
	}

	syscall_services[num] = service;
	kprintf(LOG_NORMAL, "syscall: registered system call service %" PRIu16 "\n", num);
	rwlock_unlock(&syscall_services_lock);
	return 0;
}
