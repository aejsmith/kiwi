OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(__rtld_entry)

SECTIONS {
	. = 0x200000 + SIZEOF_HEADERS;
	__executable_start = .;

	.text : {
		*(.text)
		*(.text.*)
		. = ALIGN(0x1000);
		_etext = .;
	}

	.rodata ALIGN(0x1000) : {
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(0x1000);
	}

	.data ALIGN(0x1000) : {
		*(.data)
		*(.data.*)
		. = ALIGN(0x1000);
		_edata = .;
	}

	.bss ALIGN(0x1000) : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(0x1000);
	}

	_end = .;

	/DISCARD/ : {
		*(.note.GNU-stack)
		*(.comment*)
	}
}
