ENTRY(__kernel_entry)
OUTPUT_ARCH("i386:x86-64")
OUTPUT_FORMAT("elf64-x86-64")

SECTIONS {
	. = 0xFFFFFFFF80000000 + SIZEOF_HEADERS;

	.text : {
		__text_start = .;
		*(.multiboot)
		*(.text)
		*(.text.*)
		. = ALIGN(0x1000);
		__text_end = .;
	}

	.init ALIGN(0x1000) : {
		__init_start = .;
		*(.init.data)
		*(.init.text)

		__ap_trampoline_start = .;
		*(.init.trampoline)
		__ap_trampoline_end = .;

		. = ALIGN(0x1000);
		__init_end = .;
	}

	.rodata ALIGN(0x1000) : {
		__rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(0x1000);
		__rodata_end = .;
	}

	.data ALIGN(0x1000) : {
		__data_start = .;
		*(.data)
		*(.data.*)

		__ctors_start = .;
		*(.ctors)
		__ctors_end = .;

		. = ALIGN(0x1000);
		__data_end = .;
	}

	.bss ALIGN(0x1000) : {
		__bss_start = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(0x1000);
		__bss_end = .;
	}

	__end = .;

	/DISCARD/ : {
		*(.note.GNU-stack)
		*(.comment*)
	}
}
