/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Disk image combination utility.
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MBR_SIGNATURE           0xaa55

#define MBR_PARTITION_TYPE_EXT2 0x83
#define MBR_PARTITION_TYPE_EFI  0xef

typedef struct mbr_partition {
    uint8_t bootable;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;
    uint32_t num_sectors;
} __attribute__((packed)) mbr_partition_t;

typedef struct mbr {
    uint8_t bootcode[446];
    mbr_partition_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

/* Use 4K physical block sizes for alignment as this is better for disks with
 * large physical block sizes. */ 
static const uint64_t logical_block_size  = 512;
static const uint64_t physical_block_size = 4096;

extern unsigned char mbr_bin[];
extern unsigned int mbr_bin_size;

static void lba_to_chs(uint64_t lba, uint8_t *_c, uint8_t *_h, uint8_t *_s) {
    const uint64_t hpc = 16;
    const uint64_t spt = 63;

    *_c = lba / (hpc * spt);
    *_h = (lba / spt) % hpc;
    *_s = (lba % spt) + 1;
}

static bool fill_partition(mbr_partition_t *partition, uint64_t offset, uint64_t size, uint8_t type, bool bootable) {
    uint64_t start_lba   = offset / logical_block_size;
    uint64_t num_sectors = size / logical_block_size;

    if (start_lba > UINT32_MAX || num_sectors > UINT32_MAX) {
        fprintf(stderr, "Partition LBA exceeds 32-bit MBR limit\n");
        return false;
    }

    partition->type        = type;
    partition->bootable    = (bootable) ? 0x80 : 0;
    partition->start_lba   = start_lba;
    partition->num_sectors = num_sectors;

    lba_to_chs(start_lba, &partition->start_cylinder, &partition->start_head, &partition->start_sector);
    lba_to_chs(start_lba + num_sectors, &partition->end_cylinder, &partition->end_head, &partition->end_sector);

    return true;
}

static bool write_image(int output_fd, int source_fd, uint64_t offset, uint64_t size) {
    ssize_t ret;

    const size_t block_size = 1 * 1024 * 1024;
    void *block = malloc(block_size);
    if (!block) {
        perror("malloc");
        return false;
    }

    uint64_t source_offset = 0;

    while (size > 0) {
        ssize_t curr_size = (size > block_size) ? block_size : size;

        ret = pread(source_fd, block, curr_size, source_offset);
        if (ret < curr_size) {
            perror("pread");
            free(block);
            return false;
        }

        ret = pwrite(output_fd, block, curr_size, offset);
        if (ret < curr_size) {
            perror("pread");
            free(block);
            return false;
        }

        size          -= curr_size;
        offset        += curr_size;
        source_offset += curr_size;
    }

    free(block);
    return true;
}

int main(int argc, char **argv) {
    int ret;
    if (argc != 4) {
        fprintf(stderr, "Usage: image_tool <output image> <EFI image> <system image>\n");
        return EXIT_FAILURE;
    }

    const char *output_path = argv[1];
    const char *efi_path    = argv[2];
    const char *system_path = argv[3];

    int efi_fd = open(efi_path, O_RDONLY);
    if (efi_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct stat efi_stat = {};
    ret = fstat(efi_fd, &efi_stat);
    if (ret != 0) {
        perror("fstat");
        return EXIT_FAILURE;
    }

    uint64_t efi_size = efi_stat.st_size;

    int system_fd = open(system_path, O_RDONLY);
    if (system_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct stat system_stat = {};
    ret = fstat(system_fd, &system_stat);
    if (ret != 0) {
        perror("fstat");
        return EXIT_FAILURE;
    }

    uint64_t system_size = system_stat.st_size;

    /* Validate image sizes. */
    if (!efi_size || !system_size ||
        efi_size % logical_block_size || system_size % logical_block_size)
    {
        fprintf(stderr, "Image sizes are invalid\n");
        return EXIT_FAILURE;
    }

    mbr_t image_mbr = {};
    memcpy(image_mbr.bootcode, mbr_bin, mbr_bin_size);
    image_mbr.signature = MBR_SIGNATURE;

    uint64_t efi_offset = physical_block_size;
    fill_partition(&image_mbr.partitions[0], efi_offset, efi_size, MBR_PARTITION_TYPE_EFI, false);

    uint64_t system_offset = efi_offset + ((efi_size + (physical_block_size - 1)) & ~(physical_block_size - 1));
    fill_partition(&image_mbr.partitions[1], system_offset, system_size, MBR_PARTITION_TYPE_EXT2, true);

    int output_fd = open(output_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* Write MBR. */
    if (pwrite(output_fd, &image_mbr, sizeof(image_mbr), 0) != sizeof(image_mbr)) {
        perror("pwrite");
        return EXIT_FAILURE;
    }

    /* Write images. */
    if (!write_image(output_fd, efi_fd, efi_offset, efi_size) ||
        !write_image(output_fd, system_fd, system_offset, system_size))
    {
        return EXIT_FAILURE;
    }

    close(output_fd);
    return EXIT_SUCCESS;
}
