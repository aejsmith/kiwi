/*
 * Copyright (C) 2009-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel console functions.
 */

#include <io/device.h>
#include <io/request.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

/** Main console. */
console_t main_console;

/** Debug console. */
console_t debug_console;

/** Initialize the debug console. */
__init_text void console_early_init(void) {
	kboot_tag_video_t *video = kboot_tag_iterate(KBOOT_TAG_VIDEO, NULL);

	platform_console_early_init(video);

	if(!main_console.out) {
		/* Look for a framebuffer console. */
		if(video && video->type == KBOOT_VIDEO_LFB)
			fb_console_early_init(video);
	}
}

/** Initialize the primary console. */
__init_text void console_init(void) {
	kboot_tag_video_t *video = kboot_tag_iterate(KBOOT_TAG_VIDEO, NULL);

	if(debug_console.out && debug_console.out->init)
		debug_console.out->init(video);
	if(main_console.out && main_console.out->init)
		main_console.out->init(video);
}

/*
 * Kernel console device functions.
 */

/** Perform I/O on the kernel console device.
 * @param device	Device to perform I/O on.
 * @param handle	File handle structure.
 * @param request	I/O request.
 * @return		Status code describing result of the operation. */
static status_t kconsole_device_io(device_t *device, file_handle_t *handle,
	io_request_t *request)
{
	char *buf;
	size_t i;
	uint16_t ch;
	status_t ret;

	buf = kmalloc(request->total, MM_USER);
	if(!buf)
		return STATUS_NO_MEMORY;

	if(request->op == IO_OP_WRITE) {
		if(!main_console.out) {
			ret = STATUS_NOT_SUPPORTED;
			goto out;
		}

		ret = io_request_copy(request, buf, request->total);
		if(ret != STATUS_SUCCESS)
			goto out;

		for(i = 0; i < request->total; i++)
			main_console.out->putc(buf[i]);
	} else {
		if(!main_console.in || !main_console.in->getc) {
			ret = STATUS_NOT_SUPPORTED;
			goto out;
		}

		for(i = 0; i < request->total; i++) {
			/* TODO: Escape sequences for special keys, nonblock. */
			do {
				ret = main_console.in->getc(&ch);
				if(ret != STATUS_SUCCESS)
					goto out;
			} while(ch > 0xFF);

			buf[i] = ch;
		}

		ret = io_request_copy(request, buf, request->total);
	}

out:
	kfree(buf);
	return ret;
}

/** Kernel console device operations structure. */
static device_ops_t kconsole_device_ops = {
	.type = FILE_TYPE_CHAR,
	.io = kconsole_device_io,
};

/** Register the kernel console device. */
static __init_text void console_device_init(void) {
	status_t ret;

	ret = device_create("kconsole", device_tree_root, &kconsole_device_ops,
		NULL, NULL, 0, NULL);
	if(ret != STATUS_SUCCESS)
		fatal("Failed to register kernel console device (%d)", ret);
}

INITCALL(console_device_init);
