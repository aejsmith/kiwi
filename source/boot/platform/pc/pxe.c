/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		PXE filesystem handling.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <pc/bios.h>
#include <pc/pxe.h>

#include <console.h>
#include <disk.h>
#include <endian.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

/** Structure containing details of a PXE handle. */
typedef struct pxe_handle {
	uint16_t packet_size;		/**< Negotiated packet size. */
	uint16_t packet_number;		/**< Current packet number. */
	char path[];			/**< Path to the file. */
} tftp_handle_t;

extern int pxe_call_real(int func, uint32_t segoff);

/** Static call structures (limited stack space. */
static pxenv_tftp_open_t tftp_open_data;
static pxenv_tftp_get_fsize_t tftp_fsize_data;

/** PXE network information. */
static pxe_ip4_t pxe_your_ip;
static pxe_ip4_t pxe_server_ip;
static pxe_ip4_t pxe_gateway_ip;

/** Current TFTP handle. */
static fs_handle_t *current_tftp_file = NULL;

/** PXE entry point. */
pxe_segoff_t pxe_entry_point;

/** Call a PXE function.
 * @param func		Function to call.
 * @param linear	Linear address of data argument.
 * @return		Return code from call. */
static int pxe_call(int func, void *linear) {
	return pxe_call_real(func, LIN2SEGOFF((ptr_t)linear));
}

/** Set the current TFTP file.
 * @param handle	Handle to set to. If NULL, current will be closed.
 * @return		Whether successful. */
static bool tftp_set_current(fs_handle_t *handle) {
	tftp_handle_t *data;

	if(current_tftp_file) {
		pxe_call(PXENV_TFTP_CLOSE, &tftp_open_data);
		current_tftp_file = NULL;
	}

	if(handle) {
		data = handle->data;
		strcpy((char *)tftp_open_data.filename, data->path);
		tftp_open_data.server_ip = pxe_server_ip;
		tftp_open_data.gateway_ip = pxe_gateway_ip;
		tftp_open_data.udp_port = cpu_to_be16(PXENV_TFTP_PORT);
		tftp_open_data.packet_size = PXENV_TFTP_PACKET_SIZE;

		if(pxe_call(PXENV_TFTP_OPEN, &tftp_open_data) != PXENV_EXIT_SUCCESS || tftp_open_data.status) {
			return false;
		}

		data->packet_size = tftp_open_data.packet_size;
		data->packet_number = 0;

		current_tftp_file = handle;
	}

	return true;
}

/** Open a TFTP file.
 * @param mount		Mount to open from.
 * @param path		Path to file/directory to open.
 * @return		Pointer to handle on success, NULL on failure. */
static fs_handle_t *tftp_open(fs_mount_t *mount, const char *path) {
	fs_handle_t *handle;
	tftp_handle_t *data;
	size_t len;

	/* Maximum path length is 128. */
	if((len = strlen(path)) >= 128) {
		return NULL;
	}

	/* Create a handle structure then try to set it as the current. */
	data = kmalloc(sizeof(tftp_handle_t) + len + 1);
	strcpy(data->path, path);
	handle = fs_handle_create(mount, false, data);
	if(!tftp_set_current(handle)) {
		fs_close(handle);
		return NULL;
	}

	return handle;
}

/** Close a TFTP handle.
 * @param handle	Handle to close. */
static void tftp_close(fs_handle_t *handle) {
	if(current_tftp_file == handle) {
		tftp_set_current(NULL);
	}

	kfree(handle->data);
}

/** Read the next packet from a TFTP file.
 * @note		Reads to BIOS_MEM_BASE.
 * @param data		Handle to read from.
 * @return		Whether read successfully. */
static bool tftp_read_packet(tftp_handle_t *data) {
	pxenv_tftp_read_t read;

	read.buffer.addr = BIOS_MEM_BASE;
	read.buffer_size = data->packet_size;
	if(pxe_call(PXENV_TFTP_READ, &read) != PXENV_EXIT_SUCCESS || read.status) {
		return false;
	}

	data->packet_number++;
	return true;
}

/** Read from a TFTP file.
 * @param handle	Handle to the file.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into the file.
 * @return		Whether read successfully. */
static bool tftp_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset) {
	tftp_handle_t *data = handle->data;
	uint32_t start, end, i, size;

	/* If the file is not already open, just open it - we will be at the
	 * beginning of the file. If it is open, and the current packet is
	 * greater than the start packet, we must re-open it. */
	if(handle != current_tftp_file || data->packet_number > (offset / data->packet_size)) {
		if(!tftp_set_current(handle)) {
			return false;
		}
	}

	/* Now work out the start block and the end block. Subtract one from
	 * count to prevent end from going onto the next block when the offset
	 * plus the count is an exact multiple of the block size. */
	start = offset / data->packet_size;
	end = (offset + (count - 1)) / data->packet_size;

	/* If the current packet number is less than the start packet, seek to
	 * the start packet. */
	if(data->packet_number < start) {
		for(i = data->packet_number; i < start; i++) {
			if(!tftp_read_packet(data)) {
				return false;
			}
		}
	}

	/* If we're not starting on a block boundary, we need to do a partial
	 * transfer on the initial block to get up to a block boundary. 
	 * If the transfer only goes across one block, this will handle it. */
	if(offset % data->packet_size) {
		if(!tftp_read_packet(data)) {
			return false;
		}

		size = (start == end) ? count : data->packet_size - (size_t)(offset % data->packet_size);
		memcpy(buf, (void *)(ptr_t)(BIOS_MEM_BASE + (offset % data->packet_size)), size);
		buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / data->packet_size;
	for(i = 0; i < size; i++, buf += data->packet_size, count -= data->packet_size, start++) {
		if(!tftp_read_packet(data)) {
			return false;
		}
		memcpy(buf, (void *)BIOS_MEM_BASE, data->packet_size);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!tftp_read_packet(data)) {
			return false;
		}
		memcpy(buf, (void *)BIOS_MEM_BASE, count);
	}

	return true;
}

/** Get the size of a TFTP file.
 * @param handle	Handle to the file.
 * @return		Size of the file. */
static offset_t tftp_size(fs_handle_t *handle) {
	tftp_handle_t *data = handle->data;

	/* I'm not actually sure whether it's necessary to do this, but I'm
	 * doing it to be on the safe side. */
	tftp_set_current(NULL);

	strcpy((char *)tftp_fsize_data.filename, data->path);
	tftp_fsize_data.server_ip = pxe_server_ip;
	tftp_fsize_data.gateway_ip = pxe_gateway_ip;

	if(pxe_call(PXENV_TFTP_GET_FSIZE, &tftp_fsize_data) != PXENV_EXIT_SUCCESS || tftp_fsize_data.status) {
		return 0;
	}

	return tftp_fsize_data.file_size;
}

/** TFTP filesystem type. */
static fs_type_t tftp_fs_type = {
	.open = tftp_open,
	.close = tftp_close,
	.read = tftp_read,
	.size = tftp_size,
};

/** Detect whether booted from PXE. */
bool pxe_detect(void) {
	pxenv_get_cached_info_t ci;
	pxenv_boot_player_t *bp;
	fs_mount_t *mount;
	bios_regs_t regs;
	pxenv_t *pxenv;
	pxe_t *pxe;

	/* Use the PXE installation check function. */
	bios_regs_init(&regs);
	regs.eax = 0x5650;
	bios_interrupt(0x1A, &regs);
	if(regs.eax != 0x564E || (regs.eflags & X86_FLAGS_CF)) {
		return false;
	}

	/* Get the PXENV+ structure. */
	pxenv = (pxenv_t *)SEGOFF2LIN((regs.es << 16) | (regs.ebx & 0xFFFF));
	if(strncmp((char *)pxenv->signature, "PXENV+", 6) != 0 || !checksum_range(pxenv, pxenv->length)) {
		boot_error("PXENV+ structure is corrupt");
	}

	/* Get the !PXE structure. */
	pxe = (pxe_t *)SEGOFF2LIN(pxenv->pxe_ptr.addr);
	if(strncmp((char *)pxe->signature, "!PXE", 4) != 0 || !checksum_range(pxe, pxe->length)) {
		boot_error("!PXE structure is corrupt");
	}

	/* Save the PXE entry point. */
	pxe_entry_point = pxe->entry_point_16;
	dprintf("pxe: booting via PXE, entry point at %04x:%04x (%p)\n", pxe_entry_point.segment,
	        pxe_entry_point.offset, SEGOFF2LIN(pxe_entry_point.addr));

	/* Obtain the server IP address for use with the TFTP calls. */
	ci.packet_type = PXENV_PACKET_TYPE_DHCP_ACK;
	ci.buffer.addr = 0;
	ci.buffer_size = 0;
	if(pxe_call(PXENV_GET_CACHED_INFO, &ci) != PXENV_EXIT_SUCCESS || ci.status) {
		boot_error("Failed to get PXE network information");
	}
	bp = (pxenv_boot_player_t *)SEGOFF2LIN(ci.buffer.addr);
	pxe_your_ip = bp->your_ip;
	pxe_server_ip = bp->server_ip;
	pxe_gateway_ip = bp->gateway_ip;
	dprintf("pxe: network information:\n");
	dprintf(" your IP:    %d.%d.%d.%d\n", bp->your_ip.a[0], bp->your_ip.a[1],
	        bp->your_ip.a[2], bp->your_ip.a[3]);
	dprintf(" server IP:  %d.%d.%d.%d\n", bp->server_ip.a[0], bp->server_ip.a[1],
	        bp->server_ip.a[2], bp->server_ip.a[3]);
	dprintf(" gateway IP: %d.%d.%d.%d\n", bp->gateway_ip.a[0], bp->gateway_ip.a[1],
	        bp->gateway_ip.a[2], bp->gateway_ip.a[3]);

	/* Mount a TFTP filesystem and add a disk with it on. */
	mount = kmalloc(sizeof(fs_mount_t));
	memset(mount, 0, sizeof(fs_mount_t));
	mount->type = &tftp_fs_type;
	mount->label = kstrdup("PXE");
	mount->uuid = kstrdup("PXE");
	disk_add("pxe", 0, 0, NULL, NULL, mount, true);

	return true;
}
