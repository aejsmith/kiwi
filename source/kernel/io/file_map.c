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
 * @brief               File map.
 *
 * TODO:
 * - An AVL tree might not be the most appropriate data structure here.
 * - Slab caches for chunk allocation. Need an appropriately sized one for each
 *   map (chunk allocation size depends on block size), have a global list of
 *   available caches and create as needed.
 */

#include <io/file_map.h>

#include <lib/bitmap.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <assert.h>
#include <status.h>

/** Header structure for a single range in a file map. */
typedef struct file_map_chunk {
    avl_tree_node_t link;           /**< Link to the map. */
} file_map_chunk_t;

/** Byte size that a chunk covers. */
#define CHUNK_SIZE 262144

static slab_cache_t *file_map_cache;

/** Get the block array for a chunk. */
static inline uint64_t *chunk_blocks(file_map_t *map, file_map_chunk_t *chunk) {
    return (uint64_t *)&chunk[1];
}

/** Get the block bitmap for a chunk. */
static inline unsigned long *chunk_bitmap(file_map_t *map, file_map_chunk_t *chunk) {
    return (unsigned long *)(&chunk_blocks(map, chunk)[map->blocks_per_chunk]);
}

/** Looks up a block in a file map.
 * @param map           Map to look up in.
 * @param num           Block number to look up.
 * @param _raw          Where to store raw block number.
 * @return              Status code describing result of the operation. */
status_t file_map_lookup(file_map_t *map, uint64_t num, uint64_t *_raw) {
    mutex_lock(&map->lock);

    uint64_t chunk_num   = num / map->blocks_per_chunk;
    uint64_t chunk_entry = num % map->blocks_per_chunk;

    file_map_chunk_t *chunk = avl_tree_lookup(&map->chunks, chunk_num, file_map_chunk_t, link);
    if (!chunk) {
        size_t size =
            sizeof(file_map_chunk_t) +
            (sizeof(uint64_t) * map->blocks_per_chunk) +
            bitmap_bytes(map->blocks_per_chunk);

        chunk = kmalloc(size, MM_KERNEL | MM_ZERO);

        avl_tree_insert(&map->chunks, chunk_num, &chunk->link);
    }

    uint64_t *blocks      = chunk_blocks(map, chunk);
    unsigned long *bitmap = chunk_bitmap(map, chunk);

    if (!bitmap_test(bitmap, chunk_entry)) {
        status_t ret = map->ops->lookup(map, num, &blocks[chunk_entry]);
        if (ret != STATUS_SUCCESS) {
            mutex_unlock(&map->lock);
            return ret;
        }

        bitmap_set(bitmap, chunk_entry);
    }

    *_raw = blocks[chunk_entry];

    mutex_unlock(&map->lock);
    return STATUS_SUCCESS;
}

/** Invalidates entries in a file map.
 * @param map           Map to invalidate in.
 * @param start         Start block to invalidate from.
 * @param count         Number of blocks to invalidate. */
void file_map_invalidate(file_map_t *map, uint64_t start, uint64_t count) {
    mutex_lock(&map->lock);

    file_map_chunk_t *chunk = NULL;
    unsigned long *bitmap;

    for (uint64_t i = start; i < (start + count); ) {
        uint64_t chunk_entry = i % map->blocks_per_chunk;

        if (!chunk || chunk_entry == 0) {
            uint64_t chunk_num = i / map->blocks_per_chunk;

            chunk = avl_tree_lookup(&map->chunks, chunk_num, file_map_chunk_t, link);
            if (!chunk) {
                /* Advance to next chunk. */
                i += map->blocks_per_chunk - chunk_entry;
                continue;
            }

            bitmap = chunk_bitmap(map, chunk);
        }

        bitmap_clear(bitmap, chunk_entry);

        if (bitmap_ffs(bitmap, map->blocks_per_chunk) < 0) {
            /* Chunk is now empty, free it and advance to next chunk. */
            avl_tree_remove(&map->chunks, &chunk->link);
            kfree(chunk);

            chunk = NULL;
            i += map->blocks_per_chunk - chunk_entry;
            continue;
        }

        i++;
    }

    mutex_unlock(&map->lock);
}

/**
 * Helper function for a VM cache to read a page from a file using its file
 * map to help read from the source device. If this function is used, the
 * operations structure for the map must have read_block set. The cache's
 * private pointer must be a pointer to the file map.
 *
 * @param cache         Cache being read into.
 * @param buf           Buffer to read into.
 * @param offset        Offset into the file to read from.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_map_read_page(vm_cache_t *cache, void *buf, offset_t offset) {
    file_map_t *map = cache->data;
    status_t ret;

    assert(map);
    assert(map->ops->read_block);
    assert(!(offset % PAGE_SIZE));

    uint64_t start = offset / map->block_size;
    uint64_t count = PAGE_SIZE / map->block_size;

    for (uint64_t i = 0; i < count; i++, buf += map->block_size) {
        uint64_t raw;
        ret = file_map_lookup(map, start + i, &raw);
        if (ret != STATUS_SUCCESS)
            return ret;

        ret = map->ops->read_block(map, buf, raw);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/**
 * Helper function for a VM cache to write a page to a file using its file
 * map to help write to the source device. If this function is used, the
 * operations structure for the map must have write_block set. The cache's
 * private pointer must be a pointer to the file map.
 *
 * @param cache         Cache being written from.
 * @param buf           Buffer containing data to write.
 * @param offset        Offset into the file to write to.
 *
 * @return              Status code describing result of the operation.
 */
status_t file_map_write_page(vm_cache_t *cache, const void *buf, offset_t offset) {
    file_map_t *map = cache->data;
    status_t ret;

    assert(map);
    assert(map->ops->write_block);
    assert(!(offset % PAGE_SIZE));

    uint64_t start = offset / map->block_size;
    uint64_t count = PAGE_SIZE / map->block_size;

    for (uint64_t i = 0; i < count; i++, buf += map->block_size) {
        uint64_t raw;
        ret = file_map_lookup(map, start + i, &raw);
        if (ret != STATUS_SUCCESS)
            return ret;

        // TODO: What happens if this fails part way through? From the VM cache
        // perspective the whole write will have failed, but some blocks have
        // actually been written...
        ret = map->ops->write_block(map, buf, raw);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/**
 * VM cache operations using a file map to read/write blocks. The cache's data
 * pointer should be set to a pointer to the file map.
 */
vm_cache_ops_t file_map_vm_cache_ops = {
    .read_page  = file_map_read_page,
    .write_page = file_map_write_page,
};

static void file_map_ctor(void *obj, void *data) {
    file_map_t *map = obj;

    mutex_init(&map->lock, "file_map_lock", 0);
    avl_tree_init(&map->chunks);
}

/** Creates a new file map.
 * @param block_size    Size of one block of the file the map is for. Must be
 *                      a power of 2 less than or equal to PAGE_SIZE.
 * @param ops           Operations structure.
 * @param private       Implementation-specific private data.
 * @return              Pointer to created file map structure. */
file_map_t *file_map_create(size_t block_size, const file_map_ops_t *ops, void *private) {
    assert(block_size <= PAGE_SIZE);
    assert(block_size <= CHUNK_SIZE);
    assert(is_pow2(block_size));

    file_map_t *map = slab_cache_alloc(file_map_cache, MM_KERNEL);

    map->block_size       = block_size;
    map->blocks_per_chunk = CHUNK_SIZE / block_size;
    map->ops              = ops;
    map->private          = private;

    return map;
}

/** Destroys a file map.
 * @param map           Map to destroy. */
void file_map_destroy(file_map_t *map) {
    avl_tree_foreach_safe(&map->chunks, iter) {
        file_map_chunk_t *chunk = avl_tree_entry(iter, file_map_chunk_t, link);

        avl_tree_remove(&map->chunks, &chunk->link);
        kfree(chunk);
    }

    slab_cache_free(file_map_cache, map);
}

/** Initialize the file map slab cache. */
static __init_text void file_map_init(void) {
    file_map_cache = object_cache_create(
        "file_map_cache", file_map_t, file_map_ctor, NULL, NULL, 0,
        MM_BOOT);
}

INITCALL(file_map_init);
