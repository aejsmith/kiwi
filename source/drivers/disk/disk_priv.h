/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Disk device manager internal definitions.
 */

#ifndef __DISK_PRIV_H
#define __DISK_PRIV_H

#include <drivers/disk.h>

/** Type of a partition scanner function.
 * @param device	Device to probe.
 * @return		Whether the disk matches this partition type. */
typedef bool (*partition_probe_t)(disk_device_t *device);

extern device_ops_t disk_device_ops;

extern bool partition_probe_msdos(disk_device_t *device);

extern bool partition_probe(disk_device_t *device);
extern void partition_add(disk_device_t *parent, int id, uint64_t offset, uint64_t size);

extern status_t disk_device_read(disk_device_t *device, void *buf, size_t count,
                                 offset_t offset, size_t *bytesp);
extern status_t disk_device_write(disk_device_t *device, const void *buf, size_t count,
                                  offset_t offset, size_t *bytesp);

#endif /* __DISK_PRIV_H */
