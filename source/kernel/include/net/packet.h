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
 * @brief               Network packet management.
 *
 * These functions implement an API for managing network packets. The goal is
 * to minimize copying needed when sending and receiving packets. Packets are
 * a chain of one or more data buffers, which allows new headers to be added
 * onto a packet without copying the existing data. Packets can also be offset
 * to remove headers.
 *
 * Note that access to packets is not internally synchronized, it is up to
 * their users to implement appropriate synchronization.
 */

#pragma once

#include <net/net.h>

struct net_buffer_external;
struct net_packet;
struct slab_cache;

/** Network packet buffer structure. */
typedef struct net_buffer {
    struct net_buffer *next;            /**< Next buffer in the chain (NULL at end). */
    uint32_t size;                      /**< Total size of this buffer's data. */
    uint32_t offset;                    /**< Start offset within the buffer. */
    uint32_t type;                      /**< Type of the buffer. */

    uint32_t _pad;                      /**< Padding for 8-byte alignment. */
} net_buffer_t;

enum {
    /**
     * Buffer data is stored in a separately kmalloc()'d data buffer. Cast to
     * net_buffer_kmalloc_t to get data pointer.
     */
    NET_BUFFER_TYPE_KMALLOC,

    /**
     * Buffer data is stored inline with the net_buffer_t allocation, both
     * allocated out of a slab cache. Cast to net_buffer_slab_t to get data and
     * cache to free to. This is used by layer/protocol implementations which
     * use a fixed size header, the buffers for storing these are allocated out
     * of slab caches.
     */
    NET_BUFFER_TYPE_SLAB,

    /**
     * Externally allocated data using a custom free function. Cast to
     * net_buffer_external_t to get data. This can be used for zero-copy
     * receives by creating a packet referring to memory which the network
     * device has DMA'd into. The free routine indicates that the memory can be
     * reused.
     */
    NET_BUFFER_TYPE_EXTERNAL,

    /**
     * Reference to a subset of a pre-existing packet. While the buffer exists
     * a reference is held to to the target packet, which prevents any
     * modification to it.
     */
    NET_BUFFER_TYPE_REF,
};

/** kmalloc()'d network packet buffer. */
typedef struct net_buffer_kmalloc {
    net_buffer_t buffer;

    void *data;                         /**< Data pointer. */
} net_buffer_kmalloc_t;

/** Slab network packet buffer. */
typedef struct net_buffer_slab {
    net_buffer_t buffer;

    struct slab_cache *cache;           /**< Slab cache to free to. */
    uint8_t data[];                     /**< Inline packet data. */
} net_buffer_slab_t;

/**
 * Free routine for externally allocated packet buffers. This is responsible for
 * freeing the data buffer and the net_buffer header itself.
 *
 * @param buffer        Buffer to free.
 */
typedef void (*net_buffer_external_free_t)(struct net_buffer_external *buffer);

/**
 * Externally allocated network packet buffer. This can be embedded inside
 * another structure used by the implementation of this to store any other
 * state needed to be able to free the buffer.
 */
typedef struct net_buffer_external {
    net_buffer_t buffer;

    net_buffer_external_free_t free;    /**< Free function. */
    void *data;                         /**< Pointer to the data. */
} net_buffer_external_t;

/** Packet reference network packet buffer. */
typedef struct net_buffer_ref {
    net_buffer_t buffer;

    struct net_packet *packet;          /**< Target packet. */
    uint32_t packet_offset;             /**< Offset into the target. */
} net_buffer_ref_t;

/**
 * Initialize a network buffer structure. type and size must be filled in by
 * the caller.
 */
static inline void net_buffer_init(net_buffer_t *buffer) {
    buffer->next   = NULL;
    buffer->offset = 0;
}

extern net_buffer_t *net_buffer_kmalloc(uint32_t size, unsigned mmflag, void **_data);
extern net_buffer_t *net_buffer_slab_alloc(struct slab_cache *cache, uint32_t size, unsigned mmflag, void **_data);

extern net_buffer_t *net_buffer_from_kmalloc(void *data, uint32_t size);
extern net_buffer_t *net_buffer_from_subset(struct net_packet *packet, uint32_t offset, uint32_t size);

extern void net_buffer_destroy(net_buffer_t *buffer);

/** Network packet packet structure. */
typedef struct net_packet {
    net_buffer_t *head;                 /**< First buffer in the chain. */

    /**
     * Number of users of the packet. This allows packets to be kept alive if
     * needed (e.g. to store them to be able to reassemble fragmented packets
     * later), and is also used if a new packet is created referencing a subset
     * of an existing packet.
     *
     * Packet sizes and offsets are immutable while their reference count is
     * greater than 1, since modifications might invalidate other packets which
     * refer to them.
     */
    uint16_t refcount;

    /**
     * Total size of the packet data (equal to the sum of (size - offset) for
     * all buffers in the chain).
     */
    uint32_t size;
} net_packet_t;

extern void net_packet_retain(net_packet_t *packet);
extern void net_packet_release(net_packet_t *packet);

extern net_packet_t *net_packet_create(net_buffer_t *buffer);

/**
 * Creates a new packet with a kmalloc()'d data buffer. This is a shortcut for
 * net_buffer_kmalloc() + net_packet_create().
 *
 * @see                 net_buffer_kmalloc().
 * @see                 net_packet_create().
 *
 * @return              Pointer to created packet, NULL on failure.
 */
static inline net_packet_t *net_packet_kmalloc(uint32_t size, unsigned mmflag, void *_data) {
    net_buffer_t *buffer = net_buffer_kmalloc(size, mmflag, _data);
    if (!buffer)
        return NULL;

    return net_packet_create(buffer);
}

/**
 * Creates a new packet taking ownership of a pre-kmalloc()'d data buffer. This
 * is a shortcut for net_packet_from_kmalloc() + net_packet_create().
 *
 * @see                 net_buffer_from_kmalloc().
 * @see                 net_packet_create().
 *
 * @return              Pointer to created packet.
 */
static inline net_packet_t *net_packet_from_kmalloc(void *data, uint32_t size) {
    net_buffer_t *buffer = net_buffer_from_kmalloc(data, size);
    return net_packet_create(buffer);
}

/**
 * Creates a new packet referring to a subset of an existing packet. This is a
 * shortcut for net_buffer_from_subset() + net_packet_create(). The new packet
 * can be freely modified without affecting the underlying packet.
 *
 * @see                 net_buffer_from_subset().
 * @see                 net_packet_create().
 *
 * @return              Pointer to created packet.
 */
static inline net_packet_t *net_packet_from_subset(net_packet_t *packet, uint32_t offset, uint32_t size) {
    net_buffer_t *buffer = net_buffer_from_subset(packet, offset, size);
    return net_packet_create(buffer);
}

extern void net_packet_offset(net_packet_t *packet, uint32_t offset);
extern void net_packet_prepend(net_packet_t *packet, net_buffer_t *buffer);

extern void *net_packet_data(net_packet_t *packet, uint32_t offset, uint32_t size);

extern void net_packet_copy_from(net_packet_t *packet, void *dest, uint32_t offset, uint32_t size);

extern void net_packet_cache_init(void);
