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

#include "iso9660.h"

/** Structure containing details of an ISO9660 filesystem. */
typedef struct iso9660_mount {
	int joliet_level;		/**< Joliet level. */
} iso9660_mount_t;

/** Structure containing details of an ISO9660 handle. */
typedef struct iso9660_handle {
	uint32_t data_len;		/**< Data length. */
	uint32_t extent;		/**< Extent block number. */
} iso9660_handle_t;

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

/** Create a handle from a directory record.
 * @param mount		Mount the node is from.
 * @param rec		Record to create from.
 * @return		Pointer to handle. */
static fs_handle_t *iso9660_handle_create(fs_mount_t *mount, iso9660_directory_record_t *rec) {
	iso9660_handle_t *data = kmalloc(sizeof(iso9660_handle_t));
	data->data_len = le32_to_cpu(rec->data_len_le);
	data->extent = le32_to_cpu(rec->extent_loc_le);
	return fs_handle_create(mount, (rec->file_flags & (1<<1)), data);
}

/** Mount an ISO9660 filesystem.
 * @param mount		Mount structure to fill in.
 * @return		Whether the file contains the FS. */
static bool iso9660_mount(fs_mount_t *mount) {
	iso9660_primary_volume_desc_t *pri = NULL;
	iso9660_supp_volume_desc_t *sup = NULL;
	iso9660_volume_desc_t *desc;
	iso9660_mount_t *data;
	int joliet = 0, i;
	bool ret = false;
	char *buf;

	/* Read in volume descriptors until we find the primary descriptor.
	 * I don't actually know whether there's a limit on the number of
	 * descriptors - I just put in a sane one so we don't loop for ages. */
	buf = kmalloc(ISO9660_BLOCK_SIZE);
	for(i = ISO9660_DATA_START; i < 128; i++) {
		if(!disk_read(mount->disk, buf, ISO9660_BLOCK_SIZE, i * ISO9660_BLOCK_SIZE)) {
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

	/* Store details of the filesystem in the mount structure. */
	data = mount->data = kmalloc(sizeof(iso9660_mount_t));
	data->joliet_level = joliet;

	/* Store the filesystem label and UUID. */
	pri->vol_ident[31] = 0;
	pri->sys_ident[31] = 0;
	mount->uuid = iso9660_make_uuid(pri);
	mount->label = kstrdup(strstrip((char *)pri->vol_ident));

	/* Retreive the root node. */
	if(joliet) {
		assert(sup);
		mount->root = iso9660_handle_create(mount, (iso9660_directory_record_t *)&sup->root_dir_record);
	} else {
		mount->root = iso9660_handle_create(mount, (iso9660_directory_record_t *)&pri->root_dir_record);
	}
	dprintf("iso9660: disk %s mounted (label: %s, joliet: %d, uuid: %s)\n",
	        mount->disk->name, mount->label, joliet, mount->uuid);
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

/** Close an ISO9660 handle.
 * @param handle	Handle to close. */
static void iso9660_close(fs_handle_t *handle) {
	kfree(handle->data);
}

/** Read from an ISO9660 handle.
 * @param handle	Handle to read from.
 * @param buf		Buffer to read into.
 * @param size		Size to read.
 * @param offset	Offset to read from.
 * @return		Whether the read succeeded. */
static bool iso9660_read(fs_handle_t *handle, void *buf, size_t size, offset_t offset) {
	iso9660_handle_t *data = handle->data;

	if(!size || offset >= data->data_len) {
		return 0;
	} else if((offset + size) > data->data_len) {
		return 0;
	}

	return disk_read(handle->mount->disk, buf, size, (data->extent * ISO9660_BLOCK_SIZE) + offset);
}

/** Read directory entries.
 * @param handle	Handle to directory.
 * @param cb		Callback to call on each entry.
 * @param arg		Data to pass to callback.
 * @return		Whether read successfully. */
static bool iso9660_read_dir(fs_handle_t *handle, fs_dir_read_cb_t cb, void *arg) {
	iso9660_mount_t *mount = handle->mount->data;
	iso9660_handle_t *data = handle->data;
	iso9660_directory_record_t *rec;
	char name[ISO9660_NAME_SIZE];
	uint32_t offset = 0;
	fs_handle_t *child;
	char *buf;
	bool ret;

	/* Read in all the directory data. */
	buf = kmalloc(data->data_len);
	if(!iso9660_read(handle, buf, data->data_len, 0)) {
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
		if(mount->joliet_level) {
			iso9660_parse_joliet_name(rec, name);
		} else {
			iso9660_parse_name(rec, name);
		}

		child = iso9660_handle_create(handle->mount, rec);
		ret = cb(name, child, arg);
		fs_close(child);
		if(!ret) {
			break;
		}
	}

	kfree(buf);
	return true;
}

/** ISO9660 filesystem operations structure. */
fs_type_t iso9660_fs_type = {
	.mount = iso9660_mount,
	.close = iso9660_close,
	.read = iso9660_read,
	.read_dir = iso9660_read_dir,
};
