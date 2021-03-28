/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               VirtIO network device driver.
 */

#include <device/bus/virtio/virtio.h>

#include <kernel.h>

static status_t virtio_net_init_device(virtio_device_t *device) {
    kprintf(LOG_DEBUG, "virtio_net: initializing device...\n");
    return STATUS_SUCCESS;
}

static virtio_driver_t virtio_net_driver = {
    .device_id   = VIRTIO_DEVICE_ID_NET,
    .init_device = virtio_net_init_device,
};

MODULE_NAME("virtio_net");
MODULE_DESC("VirtIO network device driver");
MODULE_DEPS(VIRTIO_MODULE_NAME);
MODULE_VIRTIO_DRIVER(virtio_net_driver);
