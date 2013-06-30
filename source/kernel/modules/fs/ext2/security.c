/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Ext2 security functions.
 *
 * @todo		ACL extended attribute support.
 */

#include <mm/malloc.h>

#include <object.h>
#include <status.h>

#include "ext2_priv.h"

/** Convert a mode to a set of rights.
 * @param mode		Mode to convert (the part of interest should be in the
 *			lowest 3 bits).
 * @return		Converted rights. */
static inline object_rights_t mode_to_rights(uint16_t mode) {
	object_rights_t rights = 0;

	if(mode & EXT2_S_IROTH) {
		rights |= FILE_RIGHT_READ;
	}
	if(mode & EXT2_S_IWOTH) {
		rights |= FILE_RIGHT_WRITE;
	}
	if(mode & EXT2_S_IXOTH) {
		rights |= FILE_RIGHT_EXECUTE;
	}
	return rights;
}

/** Get security attributes for an Ext2 inode.
 * @param inode		Inode to get security attributes for.
 * @param securityp	Where to store pointer to security attributes structure.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_security(ext2_inode_t *inode, object_security_t **securityp) {
	object_security_t *security;
	object_rights_t rights;
	uint16_t mode;

	security = kmalloc(sizeof(*security), MM_WAIT);
	security->uid = le16_to_cpu(inode->disk.i_uid);
	security->gid = le16_to_cpu(inode->disk.i_gid);

	security->acl = kmalloc(sizeof(*security->acl), MM_WAIT);
	object_acl_init(security->acl);
	mode = le16_to_cpu(inode->disk.i_mode);

	rights = mode_to_rights((mode & EXT2_S_IRWXU) >> 6);
	object_acl_add_entry(security->acl, ACL_ENTRY_USER, -1, rights);
	rights = mode_to_rights((mode & EXT2_S_IRWXG) >> 3);
	object_acl_add_entry(security->acl, ACL_ENTRY_GROUP, -1, rights);
	rights = mode_to_rights(mode & EXT2_S_IRWXO);
	object_acl_add_entry(security->acl, ACL_ENTRY_OTHERS, 0, rights);

	*securityp = security;
	return STATUS_SUCCESS;
}

/** Convert a set of rights to a mode.
 * @param rights	Rights to convert.
 * @return		Converted mode (only lowest 3 bits). */
static inline uint16_t rights_to_mode(object_rights_t rights) {
	uint16_t mode = 0;

	if(rights & FILE_RIGHT_READ) {
		mode |= EXT2_S_IROTH;
	}
	if(rights & FILE_RIGHT_WRITE) {
		mode |= EXT2_S_IWOTH;
	}
	if(rights & FILE_RIGHT_EXECUTE) {
		mode |= EXT2_S_IXOTH;
	}
	return mode;
}

/** Set security attributes for an Ext2 inode.
 * @param inode		Inode to set security attributes for.
 * @param security	New security attributes to set.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_set_security(ext2_inode_t *inode, const object_security_t *security) {
	uint16_t mode;
	size_t i;

	/* Convert the ACL entries into mode bits. */
	if(security->acl) {
		/* Clear permission bits from the current mode. */
		mode = le16_to_cpu(inode->disk.i_mode) & ~(EXT2_S_IRWXU | EXT2_S_IRWXG | EXT2_S_IRWXO);

		for(i = 0; i < security->acl->count; i++) {
			switch(security->acl->entries[i].type) {
			case ACL_ENTRY_USER:
				if(security->acl->entries[i].value != -1) {
					return STATUS_NOT_IMPLEMENTED;
				}

				mode |= (rights_to_mode(security->acl->entries[i].rights) << 6);
				break;
			case ACL_ENTRY_GROUP:
				if(security->acl->entries[i].value != -1) {
					return STATUS_NOT_IMPLEMENTED;
				}

				mode |= (rights_to_mode(security->acl->entries[i].rights) << 3);
				break;
			case ACL_ENTRY_OTHERS:
				mode |= rights_to_mode(security->acl->entries[i].rights);
				break;
			}
		}

		inode->disk.i_mode = cpu_to_le16(mode);
	}

	if(security->uid >= 0) {
		inode->disk.i_uid = cpu_to_le16(security->uid);
	}
	if(security->gid >= 0) {
		inode->disk.i_gid = cpu_to_le16(security->gid);
	}
	ext2_inode_flush(inode);
	return STATUS_SUCCESS;
}
