/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Kernel library heap functions.
 */

#include <core/mutex.h>

#include <kernel/vm.h>

#include <stdlib.h>
#include <string.h>

#include "libkernel.h"

/** Structure representing an area on the heap. */
typedef struct heap_chunk {
    core_list_t header;             /**< Link to chunk list. */
    size_t size;                    /**< Size of chunk including struct (low bit == used). */
    bool allocated;                 /**< Whether the chunk is allocated. */
} heap_chunk_t;

/** Lock to protect the heap. */
static CORE_MUTEX_DEFINE(heap_lock);

/** Statically allocated heap. */
static CORE_LIST_DEFINE(heap_chunks);

/** Map a new chunk.
 * @param size          Minimum size required.
 * @return              Created chunk, NULL on failure. */
static heap_chunk_t *map_chunk(size_t size) {
    heap_chunk_t *chunk;
    status_t ret;

    size = core_round_up(size, page_size);

    ret = kern_vm_map(
        (void **)&chunk, size, 0, VM_ADDRESS_ANY, VM_ACCESS_READ | VM_ACCESS_WRITE,
        VM_MAP_PRIVATE, INVALID_HANDLE, 0, "libkernel_heap");
    if (ret != STATUS_SUCCESS)
        return NULL;

    chunk->size = size;
    chunk->allocated = false;
    core_list_init(&chunk->header);
    core_list_append(&heap_chunks, &chunk->header);
    return chunk;
}

/** Allocate memory from the heap.
 * @param size          Size of allocation to make.
 * @return              Address of allocation, NULL on failure. */
void *malloc(size_t size) {
    heap_chunk_t *chunk = NULL, *new;
    size_t total;

    if (!size)
        return 0;

    /* Align all allocations to 8 bytes. */
    size = core_round_up(size, 8);
    total = size + sizeof(heap_chunk_t);

    core_mutex_lock(&heap_lock, -1);

    /* Search for a free chunk. */
    core_list_foreach(&heap_chunks, iter) {
        chunk = core_list_entry(iter, heap_chunk_t, header);
        if (!chunk->allocated && chunk->size >= total) {
            break;
        } else {
            chunk = NULL;
        }
    }

    if (!chunk) {
        chunk = map_chunk(total);
        if (!chunk) {
            core_mutex_unlock(&heap_lock);
            return NULL;
        }
    }

    /* Resize the chunk if it is too big. There must be space for a second chunk
     * header afterwards. */
    if (chunk->size >= (total + sizeof(heap_chunk_t))) {
        new = (heap_chunk_t *)((char *)chunk + total);
        new->size = chunk->size - total;
        new->allocated = false;
        core_list_init(&new->header);
        core_list_add_after(&chunk->header, &new->header);
        chunk->size = total;
    }

    chunk->allocated = true;

    core_mutex_unlock(&heap_lock);
    return ((char *)chunk + sizeof(heap_chunk_t));
}

/** Resize a memory allocation.
 * @param addr          Address of old allocation.
 * @param size          New size of allocation.
 * @return              Address of allocation, NULL on failure or if size is 0. */
void *realloc(void *addr, size_t size) {
    void *new;
    heap_chunk_t *chunk;

    if (size == 0) {
        free(addr);
        return NULL;
    } else {
        new = malloc(size);
        if (addr) {
            chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
            memcpy(new, addr, core_min(chunk->size - sizeof(heap_chunk_t), size));
            free(addr);
        }

        return new;
    }
}

/** Allocate zero-filled memory.
 * @param nmemb         Number of elements.
 * @param size          Size of each element.
 * @return              Address of allocation, NULL on failure. */
void *calloc(size_t nmemb, size_t size) {
    void *ret;

    ret = malloc(nmemb * size);
    if (ret)
        memset(ret, 0, nmemb * size);

    return ret;
}

/** Free memory allocated with malloc().
 * @param addr          Address of allocation. */
void free(void *addr) {
    heap_chunk_t *chunk, *adj;

    if (!addr)
        return;

    core_mutex_lock(&heap_lock, -1);

    chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
    if (!chunk->allocated) {
        printf("libkernel: double free on internal heap (%p)\n", addr);
        libkernel_abort();
    }

    chunk->allocated = false;

    /* Coalesce adjacent free segments. */
    if (chunk->header.next != &heap_chunks) {
        adj = core_list_entry(chunk->header.next, heap_chunk_t, header);
        if (!adj->allocated && adj == (heap_chunk_t *)((char *)chunk + chunk->size)) {
            chunk->size += adj->size;
            core_list_remove(&adj->header);
        }
    }
    if (chunk->header.prev != &heap_chunks) {
        adj = core_list_entry(chunk->header.prev, heap_chunk_t, header);
        if (!adj->allocated && chunk == (heap_chunk_t *)((char *)adj + adj->size)) {
            adj->size += chunk->size;
            core_list_remove(&chunk->header);
        }
    }

    core_mutex_unlock(&heap_lock);
}
