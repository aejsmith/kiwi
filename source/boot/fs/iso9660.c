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
 * @brief		ISO9660 filesystem support.
 */

#include <boot/console.h>
#include <boot/memory.h>

#include <lib/ctype.h>
#include <lib/string.h>

#include <assert.h>
#include <endian.h>
#include <errors.h>

#include "iso9660.h"

/** Structure containing details of an ISO9660 filesystem. */
typedef struct iso9660_filesystem {
	int joliet_level;		/**< Joliet level. */
} iso9660_filesystem_t;

/** Structure containing details of an ISO9660 node. */
typedef struct iso9660_node {
	uint32_t data_len;		/**< Data length. */
	uint32_t extent;		/**< Extent block number. */
} iso9660_node_t;

/** Convert a wide character to a multibyte sequence. */
static int utf8_wctomb(uint8_t *s, uint32_t wc, size_t max) {
	unsigned int bits, j, k;

	if(s == NULL) {
		return (wc >= 0x80);
	} else if(wc < 0x00000080) {
		*s = wc;
		return 1;
	}

	if(wc >= 0x04000000) {
		bits = 30;
		*s = 0xFC;
		j = 6;
	} else if(wc >= 0x00200000) {
		bits = 24;
		*s = 0xF8;
		j = 5;
	} else if(wc >= 0x00010000) {
		bits = 18;
		*s = 0xF0;
		j = 4;
	} else if(wc >= 0x00000800) {
		bits = 12;
		*s = 0xE0;
		j = 3;
	} else if(wc >= 0x00000080) {
		bits = 6;
		*s = 0xC0;
		j = 2;
	}

	if(j > max) {
		return -1;
	}

	*s |= (unsigned char)(wc >> bits);
	for(k = 1; k < j; k++) {
		bits -= 6;
		s[k] = 0x80 + ((wc >> bits) & 0x3f);
	}

	return k;
}

/** Convert big endian wide character string to UTF8. */
static int wcsntombs_be(uint8_t *s, uint8_t *pwcs, int inlen, int maxlen) {
	const uint8_t *ip;
	uint8_t *op;
	uint16_t c;
	int size;

	op = s;
	ip = pwcs;
	while((*ip || ip[1]) && (maxlen > 0) && (inlen > 0)) {
		c = (*ip << 8) | ip[1];
		if(c > 0x7f) {
			size = utf8_wctomb(op, c, maxlen);
			if(size == -1) {
				maxlen--;
			} else {
				op += size;
				maxlen -= size;
			}
		} else {
			*op++ = (uint8_t)c;
		}
		ip += 2;
		inlen--;
	}
	return op - s;
}

/** Work out an ISO9660 node number.
 * @param extent	Location of extent containing directory record.
 * @param offset	Offset into extent of directory record.
 * @return		Inode number. */
static inode_t iso9660_node_num(uint32_t extent, uint32_t offset) {
	return ((inode_t)extent << 32) | (inode_t)offset;
}

/** Get location of a directory record from a node number.
 * @param num		Node number to convert.
 * @param extentp	Where to store extent containing directory record.
 * @param offsetp	Where to store offset into extent of record. */
static void iso9660_record_location(inode_t num, uint32_t *extentp, uint32_t *offsetp) {
	*extentp = num >> 32;
	*offsetp = num & 0xFFFFFFFF;
}

/** Parse a name from a directory record.
 * @param record	Record to parse.
 * @param buf		Buffer to write into. */
static void iso9660_parse_name(iso9660_directory_record_t *record, char *buf) {
	uint32_t i, len;

	len = (record->file_ident_len < ISO9660_MAX_NAME_LEN) ? record->file_ident_len : ISO9660_MAX_NAME_LEN;

	for(i = 0; i < len; i++) {
		if(record->file_ident[i] == ISO9660_SEPARATOR2) {
			break;
		} else {
			buf[i] = tolower(record->file_ident[i]);
		}
	}

	if(i && buf[i - 1] == ISO9660_SEPARATOR1) {
		i--;
	}
	buf[i] = 0;
}

/** Parse a Joliet name from a directory record.
 * @param record	Record to parse.
 * @param buf		Buffer to write into. */
static void iso9660_parse_joliet_name(iso9660_directory_record_t *record, char *buf) {
	unsigned char len = wcsntombs_be(
		(uint8_t *)buf,
		record->file_ident,
		record->file_ident_len >> 1,
		ISO9660_NAME_SIZE
	);
	if((len > 2) && (buf[len - 2] == ';') && (buf[len - 1] == '1')) {
		len -= 2;
	}

	while(len >= 2 && (buf[len - 1] == '.')) {
		len--;
	}

	buf[len] = 0;
}

/** Get the length of a string with all whitespace removed.
 * @param str		String to get length of.
 * @return		Length of string with all whitespace removed. */
static size_t strlennospace(const char *str) {
	size_t len = 0;
	while(*str) {
		if(!isspace(*(str++))) {
			len++;
		}
	}
	return len;
}

/** Copy a string without whitespace.
 * @param dest		Destination.
 * @param src		Source string. */
static void strcpynospace(char *dest, const char *src) {
	char ch;

	while(*src) {
		ch = *(src++);
		if(!isspace(ch)) {
			*(dest++) = ch;
		}
	}

	*dest = 0;
}

/** Generate a UUID.
 * @param pri		Primary volume descriptor.
 * @return		Pointer to allocated string for UUID. */
static char *iso9660_make_uuid(iso9660_primary_volume_desc_t *pri) {
	char *date = (char *)pri->vol_cre_time.year;
	char *label = (char *)pri->vol_ident;
	char *sys = (char *)pri->sys_ident;
	char *uuid;

	uuid = kmalloc(strlennospace(label) + strlennospace(sys) + 16 + 1);
	strcpynospace(uuid, label);
	strcpynospace(uuid + strlen(uuid), sys);
	sprintf(uuid + strlen(uuid), "%16.16s", date);
	return uuid;
}

/** Create a node from a directory record.
 * @param fs		Filesystem the node is from.
 * @param id		ID to give the node.
 * @param rec		Record to create from.
 * @return		Pointer to node. */
static vfs_node_t *iso9660_node_create(vfs_filesystem_t *fs, inode_t id, iso9660_directory_record_t *rec) {
	int type = (rec->file_flags & (1<<1)) ? VFS_NODE_DIR : VFS_NODE_FILE;
	iso9660_node_t *node;

	/* Create a structure to store the details needed. */
	node = kmalloc(sizeof(iso9660_node_t));
	node->data_len = le32_to_cpu(rec->data_len_le);
	node->extent = le32_to_cpu(rec->extent_loc_le);

	/* Return a VFS node. */
	return vfs_node_alloc(fs, id, type, node->data_len, node);
}

/** Mount an ISO9660 filesystem.
 * @param fs		Filesystem object to fill in.
 * @return		Whether the file contains the FS. */
static bool iso9660_mount(vfs_filesystem_t *fs) {
	iso9660_primary_volume_desc_t *pri = NULL;
	iso9660_supp_volume_desc_t *sup = NULL;
	iso9660_volume_desc_t *desc;
	iso9660_filesystem_t *data;
	int joliet = 0, i;
	bool ret = false;
	char *buf;

	/* Read in volume descriptors until we find the primary descriptor.
	 * I don't actually know whether there's a limit on the number of
	 * descriptors - I just put in a sane one so we don't loop for ages. */
	buf = kmalloc(ISO9660_BLOCK_SIZE);
	for(i = ISO9660_DATA_START; i < 128; i++) {
		if(!disk_read(fs->disk, buf, ISO9660_BLOCK_SIZE, i * ISO9660_BLOCK_SIZE)) {
			goto out;
		}

		/* Check that the identifier is valid. */
		desc = (iso9660_volume_desc_t *)buf;
		if(strncmp((char *)desc->ident, "CD001", 5) != 0) {
			goto out;
		}

		if(desc->type == ISO9660_VOL_DESC_PRIMARY) {
			pri = kmalloc(sizeof(iso9660_primary_volume_desc_t));
			memcpy(pri, buf, sizeof(iso9660_primary_volume_desc_t));
		} else if(desc->type == ISO9660_VOL_DESC_SUPPLEMENTARY) {
			/* Determine whether Joliet is supported. */
			sup = (iso9660_supp_volume_desc_t *)desc;
			if(sup->esc_sequences[0] == 0x25 && sup->esc_sequences[1] == 0x2F) {
				if(sup->esc_sequences[2] == 0x40) {
					joliet = 1;
				} else if(sup->esc_sequences[2] == 0x43) {
					joliet = 2;
				} else if(sup->esc_sequences[2] == 0x45) {
					joliet = 3;
				} else {
					continue;
				}

				sup = kmalloc(sizeof(iso9660_supp_volume_desc_t));
				memcpy(sup, buf, sizeof(iso9660_supp_volume_desc_t));
			} else {
				sup = NULL;
			}
		} else if(desc->type == ISO9660_VOL_DESC_TERMINATOR) {
			break;
		}
	}

	/* Check whether a descriptor was found. */
	if(!pri) {
		goto out;
	}

	/* Store details of the filesystem in the filesystem structure. */
	data = fs->data = kmalloc(sizeof(iso9660_filesystem_t));
	data->joliet_level = joliet;

	/* Store the filesystem label and UUID. */
	pri->vol_ident[31] = 0;
	pri->sys_ident[31] = 0;
	fs->uuid = iso9660_make_uuid(pri);
	fs->label = kstrdup(strstrip((char *)pri->vol_ident));

	/* Retreive the root node. */
	if(joliet) {
		assert(sup);
		fs->root = iso9660_node_create(fs, 0, (iso9660_directory_record_t *)&sup->root_dir_record);
	} else {
		fs->root = iso9660_node_create(fs, 0, (iso9660_directory_record_t *)&pri->root_dir_record);
	}
	dprintf("iso9660: disk 0x%x mounted (label: %s, joliet: %d, uuid: %s)\n",
	        fs->disk->id, fs->label, joliet, fs->uuid);
	ret = true;
out:
	if(pri) {
		kfree(pri);
	}
	if(sup) {
		kfree(sup);
	}
	kfree(buf);
	return ret;
}

/** Read a node from a filesystem.
 * @param fs		Filesystem to get from.
 * @param id		ID of node to read.
 * @return		Pointer to node on success, NULL on failure. */
static vfs_node_t *iso9660_node_get(vfs_filesystem_t *fs, inode_t id) {
	iso9660_directory_record_t record;
	uint32_t extent, offset;

	/* Convert the node number to the directory record location. */
	iso9660_record_location(id, &extent, &offset);

	/* Read it in. */
	if(!disk_read(fs->disk, &record, sizeof(iso9660_directory_record_t),
	              (extent * ISO9660_BLOCK_SIZE) + offset)) {
		return false;
	}

	/* Create the node. */
	return iso9660_node_create(fs, id, &record);
}

/** Read data from a file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param size		Size to read.
 * @param offset	Offset to read from.
 * @return		Whether the read succeeded. */
static bool iso9660_file_read(vfs_node_t *node, void *buf, size_t size, offset_t offset) {
	iso9660_node_t *data = node->data;

	if(!size || (file_size_t)offset > data->data_len) {
		return 0;
	} else if(((file_size_t)offset + size) > data->data_len) {
		return 0;
	}

	return disk_read(node->fs->disk, buf, size, (data->extent * ISO9660_BLOCK_SIZE) + offset);
}

/** Cache entries in a directory.
 * @param node		Node to cache.
 * @return		Whether succeeded in caching. */
static bool iso9660_dir_cache(vfs_node_t *node) {
	iso9660_filesystem_t *fs = node->fs->data;
	iso9660_node_t *data = node->data;
	iso9660_directory_record_t *rec;
	char name[ISO9660_NAME_SIZE];
	uint32_t offset = 0;
	char *buf;

	/* Read in all the directory data. */
	buf = kmalloc(data->data_len);
	if(!disk_read(node->fs->disk, buf, data->data_len, data->extent * ISO9660_BLOCK_SIZE)) {
		kfree(buf);
		return false;
	}

	/* Iterate through each entry. */
	while(offset < data->data_len) {
		rec = (iso9660_directory_record_t *)(buf + offset);
		offset += rec->rec_len;

		if(rec->rec_len == 0) {
			break;
		} else if(rec->file_flags & (1<<0)) {
			continue;
		} else if(rec->file_flags & (1<<1) && rec->file_ident_len == 1) {
			if(rec->file_ident[0] == 0 || rec->file_ident[0] == 1) {
				continue;
			}
		}

		/* Parse the name based on the Joliet level. */
		if(fs->joliet_level) {
			iso9660_parse_joliet_name(rec, name);
		} else {
			iso9660_parse_name(rec, name);
		}

		vfs_dir_insert(node, name, iso9660_node_num(data->extent, offset - rec->rec_len));
	}

	return true;
}

/** ISO9660 filesystem operations structure. */
vfs_filesystem_ops_t g_iso9660_filesystem_ops = {
	.mount = iso9660_mount,
	.node_get = iso9660_node_get,
	.file_read = iso9660_file_read,
	.dir_cache = iso9660_dir_cache,
};
