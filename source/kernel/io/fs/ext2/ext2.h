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
 * @brief               Ext2 filesystem internal definitions.
 *
 * Reference:
 *  - ext4 Data Structures and Algorithms
 *    https://www.kernel.org/doc/html/latest/filesystems/ext4/index.html
 */

#pragma once

#include <io/file_map.h>
#include <io/fs.h>

#include <mm/vm_cache.h>

/**
 * On-disk filesystem structures/definitions.
 */

/** Ext2 filesystem magic number. */
#define EXT2_MAGIC              0xef53

/** Ext4 extent header magic number. */
#define EXT4_EXT_MAGIC          0xf30a

/** Special block numbers. */
#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          12
#define EXT2_DIND_BLOCK         13
#define EXT2_TIND_BLOCK         14
#define EXT2_N_BLOCKS           15

/** EXT2 revision numbers. */
#define EXT2_GOOD_OLD_REV       0
#define EXT2_DYNAMIC_REV        1

/** Filesystem status flags. */
#define EXT2_ERROR_FS           0
#define EXT2_VALID_FS           1

/** File type definitions. */
#define EXT2_S_IFMT             0xf000
#define EXT2_S_IFSOCK           0xc000
#define EXT2_S_IFLNK            0xa000
#define EXT2_S_IFREG            0x8000
#define EXT2_S_IFBLK            0x6000
#define EXT2_S_IFDIR            0x4000
#define EXT2_S_IFCHR            0x2000
#define EXT2_S_IFIFO            0x1000

/** Access rights. */
#define EXT2_S_ISUID            04000
#define EXT2_S_ISGID            02000
#define EXT2_S_ISVTX            01000
#define EXT2_S_IRWXU            00700
#define EXT2_S_IRUSR            00400
#define EXT2_S_IWUSR            00200
#define EXT2_S_IXUSR            00100
#define EXT2_S_IRWXG            00070
#define EXT2_S_IRGRP            00040
#define EXT2_S_IWGRP            00020
#define EXT2_S_IXGRP            00010
#define EXT2_S_IRWXO            00007
#define EXT2_S_IROTH            00004
#define EXT2_S_IWOTH            00002
#define EXT2_S_IXOTH            00001

/** File types in directory entries. */
#define EXT2_FT_UNKNOWN         0
#define EXT2_FT_REG_FILE        1
#define EXT2_FT_DIR             2
#define EXT2_FT_CHRDEV          3
#define EXT2_FT_BLKDEV          4
#define EXT2_FT_FIFO            5
#define EXT2_FT_SOCK            6
#define EXT2_FT_SYMLINK         7
#define EXT2_FT_MAX             8

/** Reserved inode numbers. */
#define EXT2_BAD_INO            0x1
#define EXT2_ROOT_INO           0x2
#define EXT2_ACL_IDX_INO        0x3
#define EXT2_ACL_DATA_IN        0x4
#define EXT2_BOOT_LOADER_INO    0x5
#define EXT2_UNDEL_DIR_INO      0x6

/** Limitations. */
#define EXT2_NAME_MAX           256

/** Inode flags. */
#define EXT4_EXTENTS_FL         0x80000

/** Superblock backwards-incompatible feature flags. */
#define EXT2_FEATURE_INCOMPAT_COMPRESSION       0x1
#define EXT2_FEATURE_INCOMPAT_FILETYPE          0x2
#define EXT3_FEATURE_INCOMPAT_RECOVER           0x4
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV       0x8
#define EXT2_FEATURE_INCOMPAT_META_BG           0x10
#define EXT4_FEATURE_INCOMPAT_EXTENTS           0x40
#define EXT4_FEATURE_INCOMPAT_64BIT             0x80
#define EXT4_FEATURE_INCOMPAT_MMP               0x100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG           0x200
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER     0x1
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE       0x2
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR        0x4

/** Features that we support. */
#define EXT2_FEATURE_RO_COMPAT_SUPP ( \
    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | \
    EXT2_FEATURE_RO_COMPAT_LARGE_FILE | \
    EXT2_FEATURE_RO_COMPAT_BTREE_DIR)

#define EXT2_FEATURE_INCOMPAT_SUPP ( \
    EXT2_FEATURE_INCOMPAT_FILETYPE | \
    EXT2_FEATURE_INCOMPAT_META_BG | \
    EXT4_FEATURE_INCOMPAT_64BIT)

/** Structure sizes. */
#define EXT2_SUPERBLOCK_OFFSET                  1024
#define EXT2_SUPERBLOCK_SIZE                    1024
#define EXT2_INODE_SIZE                         128
#define EXT2_MIN_GROUP_DESC_SIZE                32
#define EXT2_MIN_GROUP_DESC_SIZE_64BIT          64
#define EXT2_MAX_GROUP_DESC_SIZE                1024
#define EXT2_DIRENT_SIZE                        8
#define EXT4_EXTENT_HEADER_SIZE                 12
#define EXT4_EXTENT_IDX_SIZE                    12
#define EXT4_EXTENT_SIZE                        12

/** Superblock of an Ext2 filesystem. */
typedef struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;

    /* EXT2_DYNAMIC_REV superblocks only. */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;

    /** Performance hints. */
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;

    /** Journaling support (EXT3_FEATURE_COMPAT_HAS_JOURNAL). */
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];

    /** 64-bit support (EXT4_FEATURE_INCOMPAT_64BIT). */
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_reserved_char_pad2;
    uint16_t s_reserved_pad;

    uint32_t s_reserved[162];
} __packed ext2_superblock_t;

/** Group descriptor table. */
typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap;
    uint16_t bg_block_bitmap_csum;
    uint16_t bg_inode_bitmap_csum;
    uint16_t bg_itable_unused;
    uint16_t bg_checksum;
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} __packed ext2_group_desc_t;

/** Ext2 inode structure. */
typedef struct ext2_disk_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    union {
        struct {
            uint32_t l_i_version;
        } linux1;
        struct {
            uint32_t h_i_translator;
        } hurd1;
        struct {
            uint32_t m_i_reserved1;
        } masix1;
    } osd1;
    uint32_t i_block[EXT2_N_BLOCKS];
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    union {
        struct {
            uint16_t l_i_blocks_high;
            uint16_t l_i_file_acl_high;
            uint16_t l_i_uid_high;
            uint16_t l_i_gid_high;
            uint16_t l_i_checksum_lo;
            uint16_t l_i_reserved;
        } linux2;
        struct {
            uint16_t h_i_reserved1;
            uint16_t h_i_mode_high;
            uint16_t h_i_uid_high;
            uint16_t h_i_gid_high;
            uint32_t h_i_author;
        } hurd2;
        struct {
            uint16_t h_i_reserved1;
            uint16_t m_i_file_acl_high;
            uint32_t m_i_reserved2[2];
        } masix2;
    } osd2;
    uint16_t i_extra_isize;
    uint16_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    uint32_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    uint32_t i_projid;
} __packed ext2_disk_inode_t;

/** Ext2 directory entry. */
typedef struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} __packed ext2_dir_entry_t;

/* Ext4 on-disk extent structure. */
typedef struct ext4_extent {
    uint32_t ee_block;
    uint16_t ee_len;
    uint16_t ee_start_hi;
    uint32_t ee_start;
} __packed ext4_extent_t;

/* Ext4 on-disk index structure. */
typedef struct ext4_extent_idx {
    uint32_t ei_block;
    uint32_t ei_leaf;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} __packed ext4_extent_idx_t;

/* Ext4 extent header structure. */
typedef struct ext4_extent_header {
    uint16_t eh_magic;
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} __packed ext4_extent_header_t;

/**
 * Driver internal definitions.
 */

/** Define to enable debug output. */
#define DEBUG_EXT2

#ifdef DEBUG_EXT2
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Ext2 mount structure. */
typedef struct ext2_mount {
    fs_mount_t *fs;                         /**< Parent fs_mount_t. */

    /** Superblock and information retrieved from it. */
    ext2_superblock_t sb;
    uint32_t inodes_per_group;
    uint32_t inode_count;
    uint32_t blocks_per_group;
    uint32_t block_count;
    uint32_t block_size;
    uint32_t block_groups;
    uint32_t inode_size;
    uint32_t inode_read_size;

    /** Group descriptor table. */
    uint32_t group_desc_size;
    offset_t group_table_offset;
    size_t group_table_size;
    void *group_table;
} ext2_mount_t;

/** Ext2 in-memory inode structure. */
typedef struct ext2_inode {
    ext2_mount_t *mount;                    /**< Parent mount. */
    uint32_t num;                           /**< Inode number. */
    offset_t disk_offset;                   /**< Offset on disk. */
    ext2_disk_inode_t disk;                 /**< On-disk inode structure. */
    offset_t size;                          /**< Size of inode data. */

    file_map_t *map;                        /**< File block map. */
    vm_cache_t *cache;                      /**< Page cache. */
} ext2_inode_t;

extern status_t ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t num);

/** Callback function for ext2_dir_iterate().
 * @param inode         Directory being iterated.
 * @param entry         Directory entry header.
 * @param name          Name of entry.
 * @param offset        Offset of entry.
 * @param arg           Argument passed to ext2_dir_iterate().
 * @return              Whether to continue iterating. */
typedef bool (*ext2_dir_iterate_cb_t)(
    ext2_inode_t *inode, ext2_dir_entry_t *entry, const char *name,
    offset_t offset, void *arg);

extern status_t ext2_dir_iterate(ext2_inode_t *inode, offset_t offset, ext2_dir_iterate_cb_t cb, void *arg);

extern status_t ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **_inode);
extern void ext2_inode_put(ext2_inode_t *inode);

extern nstime_t ext2_inode_atime(ext2_inode_t *inode);
extern nstime_t ext2_inode_ctime(ext2_inode_t *inode);
extern nstime_t ext2_inode_mtime(ext2_inode_t *inode);

static inline status_t ext2_inode_read(
    ext2_inode_t *inode, void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    return vm_cache_read(inode->cache, buf, size, offset, _bytes);
}

static inline status_t ext2_inode_write(
    ext2_inode_t *inode, const void *buf, size_t size, offset_t offset,
    size_t *_bytes)
{
    return vm_cache_write(inode->cache, buf, size, offset, _bytes);
}
