/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		ELF loading functions.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <elf.h>
#include <memory.h>

#ifdef CONFIG_ARCH_LITTLE_ENDIAN
# define ELF_ENDIAN	ELFDATA2LSB
#else
# define ELF_ENDIAN	ELFDATA2MSB
#endif

/** Check whether a file is a certain ELF type.
 * @param handle	Handle to file to check.
 * @param bitsize	ELF class definition.
 * @param machine	ELF machine definition.
 * @return		Whether the file is this type. */
static bool elf_check(fs_handle_t *handle, uint8_t bitsize, uint8_t machine) {
	Elf32_Ehdr ehdr;

	if(!fs_file_read(handle, &ehdr, sizeof(ehdr), 0)) {
		return false;
	} else if(strncmp((const char *)ehdr.e_ident, ELF_MAGIC, 4) != 0) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_CLASS] != bitsize) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_DATA] != ELF_ENDIAN) {
		return false;
	} else if(machine != ELF_EM_NONE && ehdr.e_machine != machine) {
		return false;
	} else if(ehdr.e_type != ELF_ET_EXEC) {
		return false;
	}
	return true;
}

/** Define an ELF note iteration function. */
#define ELF_NOTE_ITERATE(_bits) \
	static bool elf_note_iterate##_bits(fs_handle_t *handle, elf_note_iterate_t cb, void *data) { \
		Elf##_bits##_Phdr *phdrs; \
		Elf##_bits##_Ehdr ehdr; \
		elf_note_t *note; \
		const char *name; \
		size_t i, offset; \
		void *buf, *desc; \
		\
		if(!fs_file_read(handle, &ehdr, sizeof(ehdr), 0)) { \
			return false; \
		} \
		\
		phdrs = kmalloc(sizeof(*phdrs) * ehdr.e_phnum); \
		if(!fs_file_read(handle, phdrs, ehdr.e_phnum * ehdr.e_phentsize, ehdr.e_phoff)) { \
			return false; \
		} \
		\
		for(i = 0; i < ehdr.e_phnum; i++) { \
			if(phdrs[i].p_type != ELF_PT_NOTE) { \
				continue; \
			} \
			\
			buf = kmalloc(phdrs[i].p_filesz); \
			if(!fs_file_read(handle, buf, phdrs[i].p_filesz, phdrs[i].p_offset)) { \
				return false; \
			} \
			for(offset = 0; offset < phdrs[i].p_filesz; ) { \
				note = (elf_note_t *)(buf + offset); \
				offset += sizeof(elf_note_t); \
				name = (const char *)(buf + offset); \
				offset += ROUND_UP(note->n_namesz, 4); \
				desc = buf + offset; \
				offset += ROUND_UP(note->n_descsz, 4); \
				if(!cb(note, name, desc, data)) { \
					kfree(buf); \
					kfree(phdrs); \
					return true; \
				} \
			} \
			kfree(buf); \
		} \
		\
		kfree(phdrs); \
		return true; \
	}

ELF_NOTE_ITERATE(32);
ELF_NOTE_ITERATE(64);

/** Iterate over ELF notes.
 * @param handle	Handle to file.
 * @param cb		Callback function.
 * @param data		Data pointer to pass to callback.
 * @return		Whether the file is an ELF file. */
bool elf_note_iterate(fs_handle_t *handle, elf_note_iterate_t cb, void *data) {
	if(elf_check(handle, ELFCLASS32, ELF_EM_NONE)) {
		return elf_note_iterate32(handle, cb, data);
	} else if(elf_check(handle, ELFCLASS64, ELF_EM_NONE)) {
		return elf_note_iterate64(handle, cb, data);
	}

	return false;
}
