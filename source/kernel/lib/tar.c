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
 * @brief               TAR file extractor.
 */

#include <io/fs.h>

#include <mm/malloc.h>

#include <lib/string.h>
#include <lib/tar.h>
#include <lib/utility.h>

#include <kernel.h>
#include <object.h>
#include <status.h>

#if 0
/** Convert a TAR mode to a set of rights.
 * @param mode          Mode to convert (the part of interest should be in the
 *                      lowest 3 bits).
 * @return              Converted rights. */
static inline object_rights_t mode_to_rights(uint16_t mode) {
    object_rights_t rights = 0;

    if (mode & TOREAD)
        rights |= FILE_RIGHT_READ;
    if (mode & TOWRITE)
        rights |= FILE_RIGHT_WRITE;
    if (mode & TOEXEC)
        rights |= FILE_RIGHT_EXECUTE;

    return rights;
}
#endif

/** Handle an entry in a TAR file.
 * @param header        Header for the entry.
 * @param data          Data for the entry.
 * @param size          Size of data.
 * @param prefix        Prefix for path string.
 * @return              Status code describing result of the operation. */
static status_t handle_tar_entry(tar_header_t *header, void *data, size_t size, const char *prefix) {
    object_handle_t *handle;
    status_t ret;
    size_t bytes;
    char *path;

    /* Work out the path to the entry. */
    if (prefix) {
        path = kmalloc(strlen(prefix) + strlen(header->name) + 2, MM_KERNEL);
        strcpy(path, prefix);
        if (prefix[strlen(prefix) - 1] != '/')
            strcat(path, "/");
        strcat(path, header->name);
    } else {
        path = header->name;
    }

    /* Handle the entry based on its type flag. */
    switch (header->typeflag) {
    case REGTYPE:
    case AREGTYPE:
        ret = fs_open(path, FILE_ACCESS_WRITE, 0, FS_MUST_CREATE, &handle);
        if (ret != STATUS_SUCCESS)
            goto out;

        ret = file_write(handle, data, size, -1, &bytes);
        if (ret != STATUS_SUCCESS) {
            object_handle_release(handle);
            goto out;
        } else if (bytes != size) {
            ret = STATUS_DEVICE_ERROR;
            object_handle_release(handle);
            goto out;
        }

        object_handle_release(handle);
        break;
    case DIRTYPE:
        ret = fs_create_dir(path);
        if (ret != STATUS_SUCCESS)
            goto out;

        break;
    case SYMTYPE:
        ret = fs_create_symlink(path, header->linkname);
        if (ret != STATUS_SUCCESS)
            goto out;

        break;
    default:
        kprintf(LOG_DEBUG, "tar: unhandled type flag '%c'\n", header->typeflag);
        break;
    }

    ret = STATUS_SUCCESS;

out:
    if (prefix)
        kfree(path);

    return ret;
}

/** Extract a TAR file.
 * @param handle        Handle to file.
 * @param dest          If not NULL, will be prepended to path strings in the
 *                      TAR file. If NULL and any path strings are relative,
 *                      they will be extracted to the current directory.
 * @return              Status code describing result of the operation. */
status_t tar_extract(object_handle_t *handle, const char *dest) {
    tar_header_t *header;
    offset_t offset = 0;
    size_t bytes, size;
    void *data = NULL;
    status_t ret;

    header = kmalloc(sizeof(tar_header_t), MM_KERNEL);

    while (true) {
        /* Read in the next header. */
        ret = file_read(handle, header, sizeof(*header), offset, &bytes);
        if (ret != STATUS_SUCCESS) {
            goto fail;
        } else if (bytes < 2) {
            ret = (offset) ? STATUS_MALFORMED_IMAGE : STATUS_UNKNOWN_IMAGE;
            goto fail;
        }

        /* Two NULL bytes in the name field indicates EOF. */
        if (!header->name[0] && !header->name[1])
            break;

        /* Check validity of the header. */
        if (bytes != sizeof(*header) || strncmp(header->magic, "ustar", 5) != 0) {
            ret = (offset) ? STATUS_MALFORMED_IMAGE : STATUS_UNKNOWN_IMAGE;
            goto fail;
        }

        /* All fields in the header are stored as ASCII - convert the size to an
         * integer (base 8). */
        size = strtoul(header->size, NULL, 8);

        /* Read in the entry data. */
        if (size) {
            data = kmalloc(size, MM_NOWAIT);
            if (!data) {
                ret = STATUS_NO_MEMORY;
                goto fail;
            }

            ret = file_read(handle, data, size, offset + 512, &bytes);
            if (ret != STATUS_SUCCESS) {
                goto fail;
            } else if (bytes != size) {
                ret = STATUS_MALFORMED_IMAGE;
                goto fail;
            }
        }

        /* Process the entry. */
        ret = handle_tar_entry(header, data, size, dest);
        if (ret != STATUS_SUCCESS)
            goto fail;

        if (data) {
            kfree(data);
            data = NULL;
        }

        /* 512 for the header, plus the file size if necessary. */
        offset += 512;
        if (size)
            offset += round_up(size, 512);
    }

    kfree(header);
    return STATUS_SUCCESS;

fail:
    if (data)
        kfree(data);

    kfree(header);
    return ret;
}
