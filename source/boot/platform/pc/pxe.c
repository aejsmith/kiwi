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
 * @brief		PXE filesystem handling.
 */

#include <arch/cpu.h>

#include <boot/console.h>
#include <boot/disk.h>
#include <boot/error.h>
#include <boot/memory.h>
#include <boot/fs.h>

#include <lib/string.h>
#include <lib/utility.h>

#include "bios.h"
#include "pxe.h"

extern int pxe_call_real(int func, uint32_t segoff);

/** PXE network information. */
static pxe_ip4_t pxe_your_ip;
static pxe_ip4_t pxe_server_ip;
static pxe_ip4_t pxe_gateway_ip;

/** PXE entry point. */
pxe_segoff_t pxe_entry_point;

/** Call a PXE function.
 * @param func		Function to call.
 * @param linear	Linear address of data argument.
 * @return		Return code from call. */
static int pxe_call(int func, void *linear) {
	return pxe_call_real(func, LIN2SEGOFF((ptr_t)linear));
}

/** Detect whether booted from PXE. */
bool pxe_detect(void) {
	pxenv_get_cached_info_t ci;
	pxenv_boot_player_t *bp;
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
	dprintf(" your ip:    %d.%d.%d.%d\n", bp->your_ip.a[0], bp->your_ip.a[1],
	        bp->your_ip.a[2], bp->your_ip.a[3]);
	dprintf(" server ip:  %d.%d.%d.%d\n", bp->server_ip.a[0], bp->server_ip.a[1],
	        bp->server_ip.a[2], bp->server_ip.a[3]);
	dprintf(" gateway ip: %d.%d.%d.%d\n", bp->gateway_ip.a[0], bp->gateway_ip.a[1],
	        bp->gateway_ip.a[2], bp->gateway_ip.a[3]);

	boot_error("Not implemented");
}
