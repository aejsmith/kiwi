ENTRY(__phys_entry)
OUTPUT_ARCH("i386")
OUTPUT_FORMAT("elf32-i386")

SECTIONS {
	. = 0xFFC00000 + SIZEOF_HEADERS;
	__phys_entry = (__kernel_entry - 0xFFC00000) + 0x200000;

	.text : AT((ADDR(.text) - 0xFFC00000) + 0x200000) {
		__text_start = .;
		*(.multiboot)
		*(.text)
		*(.text.*)
		. = ALIGN(0x1000);
		__text_end = .;
	}

	.init ALIGN(0x1000) : AT((ADDR(.init) - 0xFFC00000) + 0x200000) {
		__init_start = .;
		*(.init.data)
		*(.init.text)

		__ap_trampoline_start = .;
		*(.init.trampoline)
		__ap_trampoline_end = .;

		. = ALIGN(0x1000);
		__init_end = .;
	}

	.rodata ALIGN(0x1000) : AT((ADDR(.rodata) - 0xFFC00000) + 0x200000) {
		__rodata_start = .;
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(0x1000);
		__rodata_end = .;
	}

	.data ALIGN(0x1000) : AT((ADDR(.data) - 0xFFC00000) + 0x200000) {
		__data_start = .;
		*(.data)
		*(.data.*)
		. = ALIGN(0x1000);
		__data_end = .;
	}

	.bss ALIGN(0x1000) : AT((ADDR(.bss) - 0xFFC00000) + 0x200000) {
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
