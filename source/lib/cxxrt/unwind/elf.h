/*
 * Copyright (C) 2007-2010 Alex Smith
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
 * @brief		ELF file types/definitions.
 */

#ifndef __ELF_H
#define __ELF_H

#ifdef KERNEL
# include <types.h>
#else
# include <stdint.h>
# define __packed	__attribute__((packed))
#endif

/** Basic 32-bit ELF types. */
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

/** Basic 64-bit ELF types. */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

#define EI_NIDENT		16

/** ELF32 file header. */
typedef struct {
	unsigned char e_ident[EI_NIDENT];	/**< ELF identiﬁcation. */
	Elf32_Half    e_type;			/**< Object ﬁle type. */
	Elf32_Half    e_machine;		/**< Machine type. */
	Elf32_Word    e_version;		/**< Object ﬁle version. */
	Elf32_Addr    e_entry;			/**< Entry point address. */
	Elf32_Off     e_phoff;			/**< Program header offset. */
	Elf32_Off     e_shoff;			/**< Section header offset. */
	Elf32_Word    e_flags;			/**< Processor-speciﬁc ﬂags. */
	Elf32_Half    e_ehsize;			/**< ELF header size. */
	Elf32_Half    e_phentsize;		/**< Size of program header entry. */
	Elf32_Half    e_phnum;			/**< Number of program header entries. */
	Elf32_Half    e_shentsize;		/**< Size of section header entry. */
	Elf32_Half    e_shnum;			/**< Number of section header entries. */
	Elf32_Half    e_shstrndx;		/**< Section name string table index. */
} __packed Elf32_Ehdr;

/** ELF64 file header. */
typedef struct {
	unsigned char e_ident[EI_NIDENT];	/**< ELF identiﬁcation. */
	Elf64_Half    e_type;			/**< Object ﬁle type. */
	Elf64_Half    e_machine;		/**< Machine type. */
	Elf64_Word    e_version;		/**< Object ﬁle version. */
	Elf64_Addr    e_entry;			/**< Entry point address. */
	Elf64_Off     e_phoff;			/**< Program header offset. */
	Elf64_Off     e_shoff;			/**< Section header offset. */
	Elf64_Word    e_flags;			/**< Processor-speciﬁc ﬂags. */
	Elf64_Half    e_ehsize;			/**< ELF header size. */
	Elf64_Half    e_phentsize;		/**< Size of program header entry. */
	Elf64_Half    e_phnum;			/**< Number of program header entries. */
	Elf64_Half    e_shentsize;		/**< Size of section header entry. */
	Elf64_Half    e_shnum;			/**< Number of section header entries. */
	Elf64_Half    e_shstrndx;		/**< Section name string table index. */
} __packed Elf64_Ehdr;

/** ELF magic number definitions. */
#define ELFMAG			"\177ELF"
#define SELFMAG			4
#define EI_MAG0			0
#define ELFMAG0			0x7f		/**< Magic number byte 0. */
#define EI_MAG1			0
#define ELFMAG1			'E'		/**< Magic number byte 1. */
#define EI_MAG2			0
#define ELFMAG2			'L'		/**< Magic number byte 2. */
#define EI_MAG3			0
#define ELFMAG3			'F'		/**< Magic number byte 3. */

/** Identification array indices. */
#define EI_CLASS		4		/**< Class. */
#define EI_DATA			5		/**< Data encoding. */
#define EI_VERSION		6		/**< Version. */
#define EI_OSABI		7		/**< OS/ABI. */

/** ELF types. */
#define ET_NONE			0		/**< No ﬁle type. */
#define ET_REL			1		/**< Relocatable object ﬁle. */
#define ET_EXEC			2		/**< Executable ﬁle. */
#define ET_DYN			3		/**< Shared object ﬁle. */
#define ET_CORE			4		/**< Core ﬁle. */

/** ELF classes. */
#define ELFCLASS32		1		/**< 32-bit. */
#define ELFCLASS64		2		/**< 64-bit. */

/** Endian types. */
#define ELFDATA2LSB		1		/**< Little-endian. */
#define ELFDATA2MSB		2		/**< Big-endian. */

#define EV_NONE			0
#define EV_CURRENT		1
#define EV_NUM			2

/** Machine types. */
#define EM_NONE			0		/**< No machine. */
#define EM_M32			1		/**< AT&T WE 32100. */
#define EM_SPARC		2		/**< SUN SPARC. */
#define EM_386			3		/**< Intel 80386. */
#define EM_68K			4		/**< Motorola m68k family. */
#define EM_88K			5		/**< Motorola m88k family. */
#define EM_860			7		/**< Intel 80860. */
#define EM_MIPS			8		/**< MIPS R3000 big-endian. */
#define EM_S370			9		/**< IBM System/370. */
#define EM_MIPS_RS3_LE		10		/**< MIPS R3000 little-endian. */
#define EM_PARISC		15		/**< HPPA. */
#define EM_VPP500		17		/**< Fujitsu VPP500. */
#define EM_SPARC32PLUS		18		/**< Sun's "v8plus". */
#define EM_960			19		/**< Intel 80960. */
#define EM_PPC			20		/**< PowerPC. */
#define EM_PPC64		21		/**< PowerPC 64-bit. */
#define EM_S390			22		/**< IBM S390. */
#define EM_V800			36		/**< NEC V800 series. */
#define EM_FR20			37		/**< Fujitsu FR20. */
#define EM_RH32			38		/**< TRW RH-32. */
#define EM_RCE			39		/**< Motorola RCE. */
#define EM_ARM			40		/**< ARM. */
#define EM_FAKE_ALPHA		41		/**< Digital Alpha. */
#define EM_SH			42		/**< Hitachi SH. */
#define EM_SPARCV9		43		/**< SPARC v9 64-bit. */
#define EM_TRICORE		44		/**< Siemens Tricore. */
#define EM_ARC			45		/**< Argonaut RISC Core. */
#define EM_H8_300		46		/**< Hitachi H8/300. */
#define EM_H8_300H		47		/**< Hitachi H8/300H. */
#define EM_H8S			48		/**< Hitachi H8S. */
#define EM_H8_500		49		/**< Hitachi H8/500. */
#define EM_IA_64		50		/**< Intel Merced. */
#define EM_MIPS_X		51		/**< Stanford MIPS-X. */
#define EM_COLDFIRE		52		/**< Motorola Coldfire. */
#define EM_68HC12		53		/**< Motorola M68HC12. */
#define EM_MMA			54		/**< Fujitsu MMA Multimedia Accelerator. */
#define EM_PCP			55		/**< Siemens PCP. */
#define EM_NCPU			56		/**< Sony nCPU embeeded RISC. */
#define EM_NDR1			57		/**< Denso NDR1 microprocessor. */
#define EM_STARCORE		58		/**< Motorola Start*Core processor. */
#define EM_ME16			59		/**< Toyota ME16 processor. */
#define EM_ST100		60		/**< STMicroelectronic ST100 processor. */
#define EM_TINYJ		61		/**< Advanced Logic Corp. Tinyj emb.fam. */
#define EM_X86_64		62		/**< AMD x86-64 architecture. */
#define EM_PDSP			63		/**< Sony DSP Processor. */
#define EM_FX66			66		/**< Siemens FX66 microcontroller. */
#define EM_ST9PLUS		67		/**< STMicroelectronics ST9+ 8/16 mc. */
#define EM_ST7			68		/**< STmicroelectronics ST7 8 bit mc. */
#define EM_68HC16		69		/**< Motorola MC68HC16 microcontroller. */
#define EM_68HC11		70		/**< Motorola MC68HC11 microcontroller. */
#define EM_68HC08		71		/**< Motorola MC68HC08 microcontroller. */
#define EM_68HC05		72		/**< Motorola MC68HC05 microcontroller. */
#define EM_SVX			73		/**< Silicon Graphics SVx. */
#define EM_ST19			74		/**< STMicroelectronics ST19 8 bit mc. */
#define EM_VAX			75		/**< Digital VAX. */
#define EM_CRIS			76		/**< Axis Communications 32-bit embedded processor. */
#define EM_JAVELIN		77		/**< Infineon Technologies 32-bit embedded processor. */
#define EM_FIREPATH		78		/**< Element 14 64-bit DSP Processor. */
#define EM_ZSP			79		/**< LSI Logic 16-bit DSP Processor. */
#define EM_MMIX			80		/**< Donald Knuth's educational 64-bit processor. */
#define EM_HUANY		81		/**< Harvard University machine-independent object files. */
#define EM_PRISM		82		/**< SiTera Prism. */
#define EM_AVR			83		/**< Atmel AVR 8-bit microcontroller. */
#define EM_FR30			84		/**< Fujitsu FR30. */
#define EM_D10V			85		/**< Mitsubishi D10V. */
#define EM_D30V			86		/**< Mitsubishi D30V. */
#define EM_V850			87		/**< NEC v850. */
#define EM_M32R			88		/**< Mitsubishi M32R. */
#define EM_MN10300		89		/**< Matsushita MN10300. */
#define EM_MN10200		90		/**< Matsushita MN10200. */
#define EM_PJ			91		/**< picoJava. */
#define EM_OPENRISC		92		/**< OpenRISC 32-bit embedded processor. */
#define EM_ARC_A5		93		/**< ARC Cores Tangent-A5. */
#define EM_XTENSA		94		/**< Tensilica Xtensa Architecture. */
#define EM_NUM			95

/** ELF OS/ABI types. */
#define ELFOSABI_NONE		0		/**< UNIX System V ABI. */
#define ELFOSABI_SYSV		0		/**< Alias. */
#define ELFOSABI_HPUX		1		/**< HP-UX. */
#define ELFOSABI_NETBSD		2		/**< NetBSD. */
#define ELFOSABI_LINUX		3		/**< Linux. */
#define ELFOSABI_SOLARIS	6		/**< Sun Solaris. */
#define ELFOSABI_AIX		7		/**< IBM AIX. */
#define ELFOSABI_IRIX		8		/**< SGI Irix. */
#define ELFOSABI_FREEBSD	9		/**< FreeBSD. */
#define ELFOSABI_TRU64		10		/**< Compaq TRU64 UNIX. */
#define ELFOSABI_MODESTO	11		/**< Novell Modesto. */
#define ELFOSABI_OPENBSD	12		/**< OpenBSD. */
#define ELFOSABI_ARM		97		/**< ARM. */
#define ELFOSABI_STANDALONE	255		/**< Standalone (embedded) application. */

/** ELF32 program header. */
typedef struct {
	Elf32_Word    p_type;			/**< Type of segment. */
	Elf32_Off     p_offset;			/**< Offset in ﬁle. */
	Elf32_Addr    p_vaddr;			/**< Virtual address in memory. */
	Elf32_Addr    p_paddr;			/**< Reserved. */
	Elf32_Word    p_filesz;			/**< Size of segment in ﬁle. */
	Elf32_Word    p_memsz;			/**< Size of segment in memory. */
	Elf32_Word    p_flags;			/**< Segment attributes. */
	Elf32_Word    p_align;			/**< Alignment of segment. */
} __packed Elf32_Phdr;

/** ELF64 program header. */
typedef struct {
	Elf64_Word  p_type;			/**< Type of segment. */
	Elf64_Word  p_flags;			/**< Segment attributes. */
	Elf64_Off   p_offset;			/**< Offset in ﬁle. */
	Elf64_Addr  p_vaddr;			/**< Virtual address in memory. */
	Elf64_Addr  p_paddr;			/**< Reserved. */
	Elf64_Xword p_filesz;			/**< Size of segment in ﬁle. */
	Elf64_Xword p_memsz;			/**< Size of segment in memory. */
	Elf64_Xword p_align;			/**< Alignment of segment. */
} __packed Elf64_Phdr;

/** Program header types. */
#define PT_NULL			0		/**< Unused entry. */
#define PT_LOAD			1		/**< Loadable segment. */
#define PT_DYNAMIC		2		/**< Dynamic linking tables. */
#define PT_INTERP		3		/**< Program interpreter path name. */
#define PT_NOTE			4		/**< Note sections. */
#define PT_SHLIB		5		/**< Reserved. */
#define PT_PHDR			6		/**< Program header table. */
#define PT_TLS			7		/**< Thread-local storage data. */
#define PT_GNU_EH_FRAME		0x6474e550	/**< GCC .eh_frame_hdr segment */
#define PT_GNU_STACK		0x6474e551	/**< Indicates stack executability */

/** Program header flags. */
#define PF_X			0x1		/**< Execute permission. */
#define PF_W			0x2		/**< Write permission. */
#define PF_R			0x4		/**< Read permission. */
#define PF_MASKOS		0x0FF00000	/**< OS-specific mask. */
#define PF_MASKPROC		0xF0000000	/**< Processor-specific mask. */

/** ELF32 section header. */
typedef struct {
	Elf32_Word    sh_name;			/**< Section name. */
	Elf32_Word    sh_type;			/**< Section type. */
	Elf32_Word    sh_flags;			/**< Section attributes. */
	Elf32_Addr    sh_addr;			/**< Virtual address in memory. */
	Elf32_Off     sh_offset;		/**< Offset in ﬁle. */
	Elf32_Word    sh_size;			/**< Size of section. */
	Elf32_Word    sh_link;			/**< Link to other section. */
	Elf32_Word    sh_info;			/**< Miscellaneous information. */
	Elf32_Word    sh_addralign;		/**< Address alignment boundary. */
	Elf32_Word    sh_entsize;		/**< Size of entries, if section has table. */
} __packed Elf32_Shdr;

/** ELF64 section header. */
typedef struct {
	Elf64_Word  sh_name;			/**< Section name. */
	Elf64_Word  sh_type;			/**< Section type. */
	Elf64_Xword sh_flags;			/**< Section attributes. */
	Elf64_Addr  sh_addr;			/**< Virtual address in memory. */
	Elf64_Off   sh_offset;			/**< Offset in ﬁle. */
	Elf64_Xword sh_size;			/**< Size of section. */
	Elf64_Word  sh_link;			/**< Link to other section. */
	Elf64_Word  sh_info;			/**< Miscellaneous information. */
	Elf64_Xword sh_addralign;		/**< Address alignment boundary. */
	Elf64_Xword sh_entsize;			/**< Size of entries, if section has table. */
} __packed Elf64_Shdr;

/** Section header types. */
#define SHT_NULL		0		/**< Marks an unused section header. */
#define SHT_PROGBITS		1		/**< Contains information deﬁned by the program. */
#define SHT_SYMTAB		2		/**< Contains a linker symbol table. */
#define SHT_STRTAB		3		/**< Contains a string table. */
#define SHT_RELA		4		/**< Contains "Rela" type relocation entries. */
#define SHT_HASH		5		/**< Contains a symbol hash table. */
#define SHT_DYNAMIC		6		/**< Contains dynamic linking tables. */
#define SHT_NOTE		7		/**< Contains note information. */
#define SHT_NOBITS		8		/**< Contains uninitialised space; does not occupy any space in the ﬁle. */
#define SHT_REL			9		/**< Contains "Rel" type relocation entries. */
#define SHT_SHLIB		10		/**< Reserved. */
#define SHT_DYNSYM		11		/**< Contains a dynamic loader symbol table. */
#define SHT_INIT_ARRAY		14		/**< Array of constructors. */
#define SHT_FINI_ARRAY		15		/**< Array of destructors. */
#define SHT_PREINIT_ARRAY	16		/**< Array of pre-constructors. */
#define SHT_GROUP		17		/**< Section group. */
#define SHT_SYMTAB_SHNDX	18		/**< Extended section indeces. */
#define	SHT_NUM			19		/**< Number of defined types. */
#define SHT_LOOS		0x60000000	/**< Start of OS-specific. */
#define SHT_HIOS		0x6FFFFFFF	/**< End of OS-specific. */
#define SHT_LOPROC		0x70000000	/**< Start of processor-specific. */
#define SHT_HIPROC		0x7FFFFFFF	/**< End of processor-specific. */
#define SHT_LOUSER		0x80000000	/**< Start of application-specific. */
#define SHT_HIUSER		0x8FFFFFFF	/**< End of application-specific. */

/** Special section indices. */
#define SHN_UNDEF		0		/**< Undefined section. */
#define SHN_LORESERVE		0xff00		/**< Start of reserved indices. */
#define SHN_LOPROC		0xff00		/**< Start of processor-specific. */
#define SHN_BEFORE		0xff00		/**< Order section before all others. */
#define SHN_AFTER		0xff01		/**< Order section after all others. */
#define SHN_HIPROC		0xff1f		/**< End of processor-specific. */
#define SHN_LOOS		0xff20		/**< Start of OS-specific. */
#define SHN_HIOS		0xff3f		/**< End of OS-specific. */
#define SHN_ABS			0xfff1		/**< Associated symbol is absolute. */
#define SHN_COMMON		0xfff2		/**< Associated symbol is common. */
#define SHN_XINDEX		0xffff		/**< Index is in extra table. */
#define SHN_HIRESERVE		0xffff		/**< End of reserved indices. */

/** Section header flags. */
#define SHF_WRITE		(1<<0)		/**< Section contains writable data. */
#define SHF_ALLOC		(1<<1)		/**< Section is allocated in memory image of program. */
#define SHF_EXECINSTR		(1<<2)		/**< Section contains executable instructions. */
#define SHF_MERGE		(1<<4)		/**< Might be merged. */
#define SHF_STRINGS		(1<<5)		/**< Contains nul-terminated strings. */
#define SHF_INFO_LINK		(1<<6)		/**< sh_info contains SHT index. */
#define SHF_LINK_ORDER		(1<<7)		/**< Preserve order after combining. */
#define SHF_GROUP		(1<<9)		/**< Section is member of a group. */
#define SHF_TLS			(1<<10)		/**< Section holds thread-local data. */
#define SHF_MASKOS		0x0FF00000	/**< These bits are reserved for environment-specific use. */
#define SHF_MASKPROC		0xF0000000	/**< These bits are reserved for processor-specific use. */

/** ELF32 dynamic table entry. */
typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word d_val;
		Elf32_Addr d_ptr;
	} d_un;
} __packed Elf32_Dyn;

/** ELF64 dynamic table entry. */
typedef struct {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword  d_val;
		Elf64_Addr   d_ptr;
	} d_un;
} __packed Elf64_Dyn;

/** Dynamic entry types. */
#define DT_NULL			0		/**< Marks end of dynamic section. */
#define DT_NEEDED		1		/**< Name of needed library. */
#define DT_PLTRELSZ		2		/**< Size in bytes of PLT relocs. */
#define DT_PLTGOT		3		/**< Processor defined value. */
#define DT_HASH			4		/**< Address of symbol hash table. */
#define DT_STRTAB		5		/**< Address of string table. */
#define DT_SYMTAB		6		/**< Address of symbol table. */
#define DT_RELA			7		/**< Address of Rela relocs. */
#define DT_RELASZ		8		/**< Total size of Rela relocs. */
#define DT_RELAENT		9		/**< Size of one Rela reloc. */
#define DT_STRSZ		10		/**< Size of string table. */
#define DT_SYMENT		11		/**< Size of one symbol table entry. */
#define DT_INIT			12		/**< Address of init function. */
#define DT_FINI			13		/**< Address of termination function. */
#define DT_SONAME		14		/**< Name of shared object. */
#define DT_RPATH		15		/**< Library search path (deprecated). */
#define DT_SYMBOLIC		16		/**< Start symbol search here. */
#define DT_REL			17		/**< Address of Rel relocs. */
#define DT_RELSZ		18		/**< Total size of Rel relocs. */
#define DT_RELENT		19		/**< Size of one Rel reloc. */
#define DT_PLTREL		20		/**< Type of reloc in PLT. */
#define DT_DEBUG		21		/**< For debugging; unspecified. */
#define DT_TEXTREL		22		/**< Reloc might modify .text. */
#define DT_JMPREL		23		/**< Address of PLT relocs. */
#define	DT_BIND_NOW		24		/**< Process relocations of object. */
#define	DT_INIT_ARRAY		25		/**< Array with addresses of init fct. */
#define	DT_FINI_ARRAY		26		/**< Array with addresses of fini fct. */
#define	DT_INIT_ARRAYSZ		27		/**< Size in bytes of DT_INIT_ARRAY. */
#define	DT_FINI_ARRAYSZ		28		/**< Size in bytes of DT_FINI_ARRAY. */
#define DT_RUNPATH		29		/**< Library search path. */
#define DT_FLAGS		30		/**< Flags for the object being loaded. */
#define DT_ENCODING		32		/**< Start of encoded range. */
#define DT_PREINIT_ARRAY	32		/**< Array with addresses of preinit fct. */
#define DT_PREINIT_ARRAYSZ	33		/**< size in bytes of DT_PREINIT_ARRAY. */
#define	DT_NUM			34		/**< Number used. */
#define DT_LOOS			0x6000000d	/**< Start of OS-specific. */
#define DT_HIOS			0x6ffff000	/**< End of OS-specific. */
#define DT_LOPROC		0x70000000	/**< Start of processor-specific. */
#define DT_HIPROC		0x7fffffff	/**< End of processor-specific. */

/** ELF32 relocation. */
typedef struct {
	Elf32_Addr    r_offset;			/**< Address of reference. */
	Elf32_Word    r_info;			/**< Symbol index and type of relocation. */
} __packed Elf32_Rel;

/** ELF32 relocation with addend. */
typedef struct {
	Elf32_Addr    r_offset;			/**< Address of reference. */
	Elf32_Word    r_info;			/**< Symbol index and type of relocation. */
	Elf32_Sword   r_addend;			/**< Constant part of expression. */
} __packed Elf32_Rela;

/** Macros to get ELF32 relocation information. */
#define ELF32_R_SYM(i)		((i)>>8)
#define ELF32_R_TYPE(i)		((i)&0xff)
#define ELF32_R_INFO(s,t)	(((s)<<8)+((i)&0xff))

/** ELF64 relocation. */
typedef struct {
	Elf64_Addr   r_offset;			/**< Address of reference. */
	Elf64_Xword  r_info;			/**< Symbol index and type of relocation. */
} __packed Elf64_Rel;

/** ELF64 relocation with addend. */
typedef struct {
	Elf64_Addr   r_offset;			/**< Address of reference. */
	Elf64_Xword  r_info;			/**< Symbol index and type of relocation. */
	Elf64_Sxword r_addend;			/**< Constant part of expression. */
} __packed Elf64_Rela;

/** Macros to get ELF64 relocation information. */
#define ELF64_R_SYM(i)		((i) >> 32)
#define ELF64_R_TYPE(i)		((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)	((((Elf64_Xword)(s)) << 32) + ((t) & 0xffffffffL))

/** Relocation types - i386. */
#define R_386_NONE		0		/**< No relocation. */
#define R_386_32		1		/**< Direct 32-bit. */
#define R_386_PC32		2		/**< PC relative 32-bit. */
#define R_386_GOT32		3		/**< 32-bit GOT entry. */
#define R_386_PLT32		4		/**< 32-bit PLT address. */
#define R_386_COPY		5		/**< Copy symbol at runtime. */
#define R_386_GLOB_DAT		6		/**< Create GOT entry. */
#define R_386_JMP_SLOT		7		/**< Create PLT entry. */
#define R_386_RELATIVE		8		/**< Adjust by program base. */
#define R_386_GOTOFF		9		/**< 32-bit offset to GOT. */
#define R_386_GOTPC		10		/**< 32-bit PC relative offset to GOT. */
#define R_386_32PLT		11
#define R_386_TLS_TPOFF		14		/**< Offset in static TLS block. */
#define R_386_TLS_IE		15		/**< Address of GOT entry for static TLS block offset. */
#define R_386_TLS_GOTIE		16		/**< GOT entry for static TLS block offset. */
#define R_386_TLS_LE		17		/**< Offset relative to static TLS block. */
#define R_386_TLS_GD		18		/**< Direct 32-bit for GNU version of general dynamic thread local data. */
#define R_386_TLS_LDM		19		/**< Direct 32-bit for GNU version of local dynamic thread local data (LE). */
#define R_386_16		20
#define R_386_PC16		21
#define R_386_8			22
#define R_386_PC8		23
#define R_386_TLS_GD_32		24		/**< Direct 32-bit for general dynamic thread local data. */
#define R_386_TLS_GD_PUSH	25		/**< Tag for pushl in GD TLS code. */
#define R_386_TLS_GD_CALL	26		/**< Relocation for call to __tls_get_addr(). */
#define R_386_TLS_GD_POP	27		/**< Tag for popl in GD TLS code. */
#define R_386_TLS_LDM_32	28		/**< Direct 32 bit for local dynamic thread local data (LE). */
#define R_386_TLS_LDM_PUSH	29		/**< Tag for pushl in LDM TLS code. */
#define R_386_TLS_LDM_CALL	30		/**< Relocation for call to __tls_get_addr() in LDM code. */
#define R_386_TLS_LDM_POP	31		/**< Tag for popl in LDM TLS code. */
#define R_386_TLS_LDO_32	32		/**< Offset relative to TLS block. */
#define R_386_TLS_IE_32		33		/**< GOT entry for negated static TLS block offset. */
#define R_386_TLS_LE_32		34		/**< Negated offset relative to static TLS block. */
#define R_386_TLS_DTPMOD32	35		/**< ID of module containing symbol. */
#define R_386_TLS_DTPOFF32	36		/**< Offset in TLS block. */
#define R_386_TLS_TPOFF32	37		/**< Negated offset in static TLS block. */
#define R_386_TLS_GOTDESC	39		/**< GOT offset for TLS descriptor. */
#define R_386_TLS_DESC_CALL	40		/**< Marker of call through TLS descriptor for relaxation. */
#define R_386_TLS_DESC		41		/**< TLS descriptor. */
#define R_386_IRELATIVE		42		/**< Adjust indirectly by program base. */

/** Relocation types - x86_64. */
#define R_X86_64_NONE		0		/**< No relocation. */
#define R_X86_64_64		1		/**< Direct 64-bit. */
#define R_X86_64_PC32		2		/**< PC relative 32-bit signed. */
#define R_X86_64_GOT32		3		/**< 32-bit GOT entry. */
#define R_X86_64_PLT32		4		/**< 32-bit PLT address. */
#define R_X86_64_COPY		5		/**< Copy symbol at runtime. */
#define R_X86_64_GLOB_DAT	6		/**< Create GOT entry. */
#define R_X86_64_JUMP_SLOT	7		/**< Create PLT entry. */
#define R_X86_64_RELATIVE	8		/**< Adjust by program base. */
#define R_X86_64_GOTPCREL	9		/**< 32-bit signed PC relative offset to GOT. */
#define R_X86_64_32		10		/**< Direct 32-bit zero-extended. */
#define R_X86_64_32S		11		/**< Direct 32-bit sign-extended. */
#define R_X86_64_16		12		/**< Direct 16-bit zero-extended. */
#define R_X86_64_PC16		13		/**< 16-bit sign-extended PC relative. */
#define R_X86_64_8		14		/**< Direct 8-bit sign-extended. */
#define R_X86_64_PC8		15		/**< 8-bit sign-extended PC relative. */
#define R_X86_64_DTPMOD64	16		/**< ID of module containing symbol. */
#define R_X86_64_DTPOFF64	17		/**< Offset in module's TLS block. */
#define R_X86_64_TPOFF64	18		/**< Offset in initial TLS block. */
#define R_X86_64_TLSGD		19		/**< 32-bit signed PC relative offset to two GOT entries (GD). */
#define R_X86_64_TLSLD		20		/**< 32-bit signed PC relative offset to two GOT entries (LD). */
#define R_X86_64_DTPOFF32	21		/**< Offset in TLS block. */
#define R_X86_64_GOTTPOFF	22		/**< 32-bit signed PC relative offset to GOT entry (IE). */
#define R_X86_64_TPOFF32	23		/**< Offset in initial TLS block. */
#define R_X86_64_PC64		24		/**< PC relative 64-bit. */
#define R_X86_64_GOTOFF64	25		/**< 64-bit offset to GOT. */
#define R_X86_64_GOTPC32	26		/**< 32 bit signed PC relative offset to GOT. */
#define R_X86_64_GOT64		27		/**< 64-bit GOT entry offset. */
#define R_X86_64_GOTPCREL64	28		/**< 64-bit PC relative offset to GOT entry. */
#define R_X86_64_GOTPC64	29		/**< 64-bit PC relative offset to GOT. */
#define R_X86_64_GOTPLT64	30		/**< Like GOT64, says PLT entry needed. */
#define R_X86_64_PLTOFF64	31		/**< 64-bit GOT relative offset to PLT entry. */
#define R_X86_64_SIZE32		32		/**< Size of symbol plus 32-bit addend. */
#define R_X86_64_SIZE64		33		/**< Size of symbol plus 64-bit addend. */
#define R_X86_64_GOTPC32_TLSDESC	34	/**< GOT offset for TLS descriptor. */
#define R_X86_64_TLSDESC_CALL	35		/**< Marker for call through TLS descriptor. */
#define R_X86_64_TLSDESC	36		/**< TLS descriptor. */
#define R_X86_64_IRELATIVE	37		/**< Adjust indirectly by program base. */

/** ELF32 symbol information. */
typedef struct {
	Elf32_Word    st_name;			/**< Symbol name. */
	Elf32_Addr    st_value;			/**< Symbol value. */
	Elf32_Word    st_size;			/**< Size of object (e.g., common). */
	unsigned char st_info;			/**< Type and Binding attributes. */
	unsigned char st_other;			/**< Reserved. */
	Elf32_Half    st_shndx;			/**< Section table index. */
} __packed Elf32_Sym;

/** ELF64 symbol information. */
typedef struct {
	Elf64_Word    st_name;			/**< Symbol name. */
	unsigned char st_info;			/**< Type and Binding attributes. */
	unsigned char st_other;			/**< Reserved. */
	Elf64_Half    st_shndx;			/**< Section table index. */
	Elf64_Addr    st_value;			/**< Symbol value. */
	Elf64_Xword   st_size;			/**< Size of object (e.g., common). */
} __packed Elf64_Sym;

/** Macros to get symbol information. */
#define ELF32_ST_BIND(i)	(((unsigned char)(i))>>4)
#define ELF32_ST_TYPE(i)	((i)&0xf)
#define ELF32_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))
#define ELF32_ST_VISIBILITY(o)	((o) & 0x03)
#define ELF64_ST_BIND(i)	(((unsigned char)(i))>>4)
#define ELF64_ST_TYPE(i)	((i)&0xf)
#define ELF64_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))
#define ELF64_ST_VISIBILITY(o)	((o) & 0x03)

/** Values for ELF_ST_BIND (symbol binding). */
#define STB_LOCAL		0		/**< Local symbol. */
#define STB_GLOBAL		1		/**< Global symbol. */
#define STB_WEAK		2		/**< Weak symbol. */
#define	STB_NUM			3		/**< Number of defined types. */
#define STB_LOOS		10		/**< Start of OS-specific. */
#define STB_HIOS		12		/**< End of OS-specific. */
#define STB_LOPROC		13		/**< Start of processor-specific. */
#define STB_HIPROC		15		/**< End of processor-specific. */

/** Values for ELF_ST_TYPE (symbol type). */
#define STT_NOTYPE		0		/**< Symbol type is unspecified. */
#define STT_OBJECT		1		/**< Symbol is a data object. */
#define STT_FUNC		2		/**< Symbol is a code object. */
#define STT_SECTION		3		/**< Symbol associated with a section. */
#define STT_FILE		4		/**< Symbol's name is file name. */
#define STT_COMMON		5		/**< Symbol is a common data object. */
#define STT_TLS			6		/**< Symbol is thread-local data object. */
#define	STT_NUM			7		/**< Number of defined types. */
#define STT_LOOS		10		/**< Start of OS-specific. */
#define STT_HIOS		12		/**< End of OS-specific. */
#define STT_LOPROC		13		/**< Start of processor-specific. */
#define STT_HIPROC		15		/**< End of processor-specific. */

/** Symbol visibility encoded in the st_other field. */
#define STV_DEFAULT		0		/**< Default symbol visibility rules. */
#define STV_INTERNAL		1		/**< Processor-specific hidden class. */
#define STV_HIDDEN		2		/**< Symbol unavailable in other modules. */
#define STV_PROTECTED		3		/**< Not preemptible. */

/** Special symbol table indices. */
#define STN_UNDEF		0		/**< End of a chain. */

/** ELF32 note information. */
typedef struct {
	Elf32_Word n_namesz;			/**< Length of the note's name. */
	Elf32_Word n_descsz;			/**< Length of the note's descriptor. */
	Elf32_Word n_type;			/**< Type of the note. */
} __packed Elf32_Note;

/** ELF32 note information. */
typedef struct {
	Elf64_Word n_namesz;			/**< Length of the note's name. */
	Elf64_Word n_descsz;			/**< Length of the note's descriptor. */
	Elf64_Word n_type;			/**< Type of the note. */
} __packed Elf64_Note;

#endif /* __ELF_H */
