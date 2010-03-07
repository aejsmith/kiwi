/*
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		ELF file types/definitions.
 */

#ifndef __ELF_H
#define __ELF_H

#include <types.h>

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

/** ELF magic number definitions. */
#define ELF_MAGIC		"\177ELF"
#define ELF_MAG0		0x7f		/**< Magic number byte 0. */
#define ELF_MAG1		'E'		/**< Magic number byte 1. */
#define ELF_MAG2		'L'		/**< Magic number byte 2. */
#define ELF_MAG3		'F'		/**< Magic number byte 3. */

/** Identification array indices. */
#define ELF_EI_CLASS		4		/**< Class. */
#define ELF_EI_DATA		5		/**< Data encoding. */
#define ELF_EI_VERSION		6		/**< Version. */
#define ELF_EI_OSABI		7		/**< OS/ABI. */

/** ELF types. */
#define ELF_ET_NONE		0		/**< No ﬁle type. */
#define ELF_ET_REL		1		/**< Relocatable object ﬁle. */
#define ELF_ET_EXEC		2		/**< Executable ﬁle. */
#define ELF_ET_DYN		3		/**< Shared object ﬁle. */
#define ELF_ET_CORE		4		/**< Core ﬁle. */

/** ELF classes. */
#define ELFCLASS32		1		/**< 32-bit. */
#define ELFCLASS64		2		/**< 64-bit. */

/** Endian types. */
#define ELFDATA2LSB		1		/**< Little-endian. */
#define ELFDATA2MSB		2		/**< Big-endian. */

/** Machine types. */
#define ELF_EM_NONE		0		/**< No machine. */
#define ELF_EM_M32		1		/**< AT&T WE 32100. */
#define ELF_EM_SPARC		2		/**< SUN SPARC. */
#define ELF_EM_386		3		/**< Intel 80386. */
#define ELF_EM_68K		4		/**< Motorola m68k family. */
#define ELF_EM_88K		5		/**< Motorola m88k family. */
#define ELF_EM_860		7		/**< Intel 80860. */
#define ELF_EM_MIPS		8		/**< MIPS R3000 big-endian. */
#define ELF_EM_S370		9		/**< IBM System/370. */
#define ELF_EM_MIPS_RS3_LE	10		/**< MIPS R3000 little-endian. */
#define ELF_EM_PARISC		15		/**< HPPA. */
#define ELF_EM_VPP500		17		/**< Fujitsu VPP500. */
#define ELF_EM_SPARC32PLUS	18		/**< Sun's "v8plus". */
#define ELF_EM_960		19		/**< Intel 80960. */
#define ELF_EM_PPC		20		/**< PowerPC. */
#define ELF_EM_PPC64		21		/**< PowerPC 64-bit. */
#define ELF_EM_S390		22		/**< IBM S390. */
#define ELF_EM_V800		36		/**< NEC V800 series. */
#define ELF_EM_FR20		37		/**< Fujitsu FR20. */
#define ELF_EM_RH32		38		/**< TRW RH-32. */
#define ELF_EM_RCE		39		/**< Motorola RCE. */
#define ELF_EM_ARM		40		/**< ARM. */
#define ELF_EM_FAKE_ALPHA	41		/**< Digital Alpha. */
#define ELF_EM_SH		42		/**< Hitachi SH. */
#define ELF_EM_SPARCV9		43		/**< SPARC v9 64-bit. */
#define ELF_EM_TRICORE		44		/**< Siemens Tricore. */
#define ELF_EM_ARC		45		/**< Argonaut RISC Core. */
#define ELF_EM_H8_300		46		/**< Hitachi H8/300. */
#define ELF_EM_H8_300H		47		/**< Hitachi H8/300H. */
#define ELF_EM_H8S		48		/**< Hitachi H8S. */
#define ELF_EM_H8_500		49		/**< Hitachi H8/500. */
#define ELF_EM_IA_64		50		/**< Intel Merced. */
#define ELF_EM_MIPS_X		51		/**< Stanford MIPS-X. */
#define ELF_EM_COLDFIRE		52		/**< Motorola Coldfire. */
#define ELF_EM_68HC12		53		/**< Motorola M68HC12. */
#define ELF_EM_MMA		54		/**< Fujitsu MMA Multimedia Accelerator. */
#define ELF_EM_PCP		55		/**< Siemens PCP. */
#define ELF_EM_NCPU		56		/**< Sony nCPU embeeded RISC. */
#define ELF_EM_NDR1		57		/**< Denso NDR1 microprocessor. */
#define ELF_EM_STARCORE		58		/**< Motorola Start*Core processor. */
#define ELF_EM_ME16		59		/**< Toyota ME16 processor. */
#define ELF_EM_ST100		60		/**< STMicroelectronic ST100 processor. */
#define ELF_EM_TINYJ		61		/**< Advanced Logic Corp. Tinyj emb.fam. */
#define ELF_EM_X86_64		62		/**< AMD x86-64 architecture. */
#define ELF_EM_PDSP		63		/**< Sony DSP Processor. */
#define ELF_EM_FX66		66		/**< Siemens FX66 microcontroller. */
#define ELF_EM_ST9PLUS		67		/**< STMicroelectronics ST9+ 8/16 mc. */
#define ELF_EM_ST7		68		/**< STmicroelectronics ST7 8 bit mc. */
#define ELF_EM_68HC16		69		/**< Motorola MC68HC16 microcontroller. */
#define ELF_EM_68HC11		70		/**< Motorola MC68HC11 microcontroller. */
#define ELF_EM_68HC08		71		/**< Motorola MC68HC08 microcontroller. */
#define ELF_EM_68HC05		72		/**< Motorola MC68HC05 microcontroller. */
#define ELF_EM_SVX		73		/**< Silicon Graphics SVx. */
#define ELF_EM_ST19		74		/**< STMicroelectronics ST19 8 bit mc. */
#define ELF_EM_VAX		75		/**< Digital VAX. */
#define ELF_EM_CRIS		76		/**< Axis Communications 32-bit embedded processor. */
#define ELF_EM_JAVELIN		77		/**< Infineon Technologies 32-bit embedded processor. */
#define ELF_EM_FIREPATH		78		/**< Element 14 64-bit DSP Processor. */
#define ELF_EM_ZSP		79		/**< LSI Logic 16-bit DSP Processor. */
#define ELF_EM_MMIX		80		/**< Donald Knuth's educational 64-bit processor. */
#define ELF_EM_HUANY		81		/**< Harvard University machine-independent object files. */
#define ELF_EM_PRISM		82		/**< SiTera Prism. */
#define ELF_EM_AVR		83		/**< Atmel AVR 8-bit microcontroller. */
#define ELF_EM_FR30		84		/**< Fujitsu FR30. */
#define ELF_EM_D10V		85		/**< Mitsubishi D10V. */
#define ELF_EM_D30V		86		/**< Mitsubishi D30V. */
#define ELF_EM_V850		87		/**< NEC v850. */
#define ELF_EM_M32R		88		/**< Mitsubishi M32R. */
#define ELF_EM_MN10300		89		/**< Matsushita MN10300. */
#define ELF_EM_MN10200		90		/**< Matsushita MN10200. */
#define ELF_EM_PJ		91		/**< picoJava. */
#define ELF_EM_OPENRISC		92		/**< OpenRISC 32-bit embedded processor. */
#define ELF_EM_ARC_A5		93		/**< ARC Cores Tangent-A5. */
#define ELF_EM_XTENSA		94		/**< Tensilica Xtensa Architecture. */
#define ELF_EM_NUM		95

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

/** Phdr types. */
#define ELF_PT_NULL		0		/**< Unused entry. */
#define ELF_PT_LOAD		1		/**< Loadable segment. */
#define ELF_PT_DYNAMIC		2		/**< Dynamic linking tables. */
#define ELF_PT_INTERP		3		/**< Program interpreter path name. */
#define ELF_PT_NOTE		4		/**< Note sections. */
#define ELF_PT_SHLIB		5		/**< Reserved. */
#define ELF_PT_PHDR		6		/**< Program header table. */
#define ELF_PT_TLS		7		/**< Thread-local storage data. */

/** Shdr types. */
#define ELF_SHT_NULL		0		/**< Marks an unused section header. */
#define ELF_SHT_PROGBITS	1		/**< Contains information deﬁned by the program. */
#define ELF_SHT_SYMTAB		2		/**< Contains a linker symbol table. */
#define ELF_SHT_STRTAB		3		/**< Contains a string table. */
#define ELF_SHT_RELA		4		/**< Contains "Rela" type relocation entries. */
#define ELF_SHT_HASH		5		/**< Contains a symbol hash table. */
#define ELF_SHT_DYNAMIC		6		/**< Contains dynamic linking tables. */
#define ELF_SHT_NOTE		7		/**< Contains note information. */
#define ELF_SHT_NOBITS		8		/**< Contains uninitialised space; does not occupy any space in the ﬁle. */
#define ELF_SHT_REL		9		/**< Contains "Rel" type relocation entries. */
#define ELF_SHT_SHLIB		10		/**< Reserved. */
#define ELF_SHT_DYNSYM		11		/**< Contains a dynamic loader symbol table. */

/** Special section header indices. */
#define ELF_SHN_UNDEF		0		/**< Undefined section. */
#define ELF_SHN_LORESERVE	0xff00		/**< Start of reserved indices. */
#define ELF_SHN_LOPROC		0xff00		/**< Start of processor-specific. */
#define ELF_SHN_BEFORE		0xff00		/**< Order section before all others. */
#define ELF_SHN_AFTER		0xff01		/**< Order section after all others. */
#define ELF_SHN_HIPROC		0xff1f		/**< End of processor-specific. */
#define ELF_SHN_LOOS		0xff20		/**< Start of OS-specific. */
#define ELF_SHN_HIOS		0xff3f		/**< End of OS-specific. */
#define ELF_SHN_ABS		0xfff1		/**< Associated symbol is absolute. */
#define ELF_SHN_COMMON		0xfff2		/**< Associated symbol is common. */
#define ELF_SHN_XINDEX		0xffff		/**< Index is in extra table. */
#define ELF_SHN_HIRESERVE	0xffff		/**< End of reserved indices. */

/** Shdr flags. */
#define ELF_SHF_WRITE		0x00000001	/**< Section contains writable data. */
#define ELF_SHF_ALLOC		0x00000002	/**< Section is allocated in memory image of program. */
#define ELF_SHF_EXECINSTR	0x00000004	/**< Section contains executable instructions. */
#define ELF_SHF_MASKOS		0x0F000000	/**< These bits are reserved for environment-specific use. */
#define ELF_SHF_MASKPROC	0xF0000000	/**< These bits are reserved for processor-specific use. */

/** Phdr flags. */
#define ELF_PF_X		0x1		/**< Execute permission. */
#define ELF_PF_W		0x2		/**< Write permission. */
#define ELF_PF_R		0x4		/**< Read permission. */

/** Dynamic entry types. */
#define ELF_DT_NULL		0		/**< Marks end of dynamic section. */
#define ELF_DT_NEEDED		1		/**< Name of needed library. */
#define ELF_DT_PLTRELSZ		2		/**< Size in bytes of PLT relocs. */
#define ELF_DT_PLTGOT		3		/**< Processor defined value. */
#define ELF_DT_HASH		4		/**< Address of symbol hash table. */
#define ELF_DT_STRTAB		5		/**< Address of string table. */
#define ELF_DT_SYMTAB		6		/**< Address of symbol table. */
#define ELF_DT_RELA		7		/**< Address of Rela relocs. */
#define ELF_DT_RELASZ		8		/**< Total size of Rela relocs. */
#define ELF_DT_RELAENT		9		/**< Size of one Rela reloc. */
#define ELF_DT_STRSZ		10		/**< Size of string table. */
#define ELF_DT_SYMENT		11		/**< Size of one symbol table entry. */
#define ELF_DT_INIT		12		/**< Address of init function. */
#define ELF_DT_FINI		13		/**< Address of termination function. */
#define ELF_DT_SONAME		14		/**< Name of shared object. */
#define ELF_DT_RPATH		15		/**< Library search path (deprecated). */
#define ELF_DT_SYMBOLIC		16		/**< Start symbol search here. */
#define ELF_DT_REL		17		/**< Address of Rel relocs. */
#define ELF_DT_RELSZ		18		/**< Total size of Rel relocs. */
#define ELF_DT_RELENT		19		/**< Size of one Rel reloc. */
#define ELF_DT_PLTREL		20		/**< Type of reloc in PLT. */
#define ELF_DT_DEBUG		21		/**< For debugging; unspecified. */
#define ELF_DT_TEXTREL		22		/**< Reloc might modify .text. */
#define ELF_DT_JMPREL		23		/**< Address of PLT relocs. */
#define	ELF_DT_BIND_NOW		24		/**< Process relocations of object. */
#define	ELF_DT_INIT_ARRAY	25		/**< Array with addresses of init fct. */
#define	ELF_DT_FINI_ARRAY	26		/**< Array with addresses of fini fct. */
#define	ELF_DT_INIT_ARRAYSZ	27		/**< Size in bytes of DT_INIT_ARRAY. */
#define	ELF_DT_FINI_ARRAYSZ	28		/**< Size in bytes of DT_FINI_ARRAY. */
#define ELF_DT_RUNPATH		29		/**< Library search path. */
#define ELF_DT_FLAGS		30		/**< Flags for the object being loaded. */
#define ELF_DT_ENCODING		32		/**< Start of encoded range. */
#define ELF_DT_PREINIT_ARRAY	32		/**< Array with addresses of preinit fct. */
#define ELF_DT_PREINIT_ARRAYSZ	33		/**< size in bytes of DT_PREINIT_ARRAY. */
#define	ELF_DT_NUM		34		/**< Number used. */
#define ELF_DT_LOOS		0x6000000d	/**< Start of OS-specific. */
#define ELF_DT_HIOS		0x6ffff000	/**< End of OS-specific. */
#define ELF_DT_LOPROC		0x70000000	/**< Start of processor-specific. */
#define ELF_DT_HIPROC		0x7fffffff	/**< End of processor-specific. */

/** Relocation types - i386. */
#define ELF_R_386_NONE		0		/**< No relocation. */
#define ELF_R_386_32		1		/**< Direct 32-bit. */
#define ELF_R_386_PC32		2		/**< PC relative 32-bit. */
#define ELF_R_386_GOT32		3		/**< 32-bit GOT entry. */
#define ELF_R_386_PLT32		4		/**< 32-bit PLT address. */
#define ELF_R_386_COPY		5		/**< Copy symbol at runtime. */
#define ELF_R_386_GLOB_DAT	6		/**< Create GOT entry. */
#define ELF_R_386_JMP_SLOT	7		/**< Create PLT entry. */
#define ELF_R_386_RELATIVE	8		/**< Adjust by program base. */
#define ELF_R_386_GOTOFF	9		/**< 32-bit offset to GOT. */
#define ELF_R_386_GOTPC		10		/**< 32-bit PC relative offset to GOT. */
#define ELF_R_386_TLS_IE	15		/**< Address of GOT entry for static TLS block offset. */

/** Relocation types - x86_64. */
#define ELF_R_X86_64_NONE	0		/**< No relocation. */
#define ELF_R_X86_64_64		1		/**< Direct 64-bit. */
#define ELF_R_X86_64_PC32	2		/**< PC relative 32-bit signed. */
#define ELF_R_X86_64_GOT32	3		/**< 32-bit GOT entry. */
#define ELF_R_X86_64_PLT32	4		/**< 32-bit PLT address. */
#define ELF_R_X86_64_COPY	5		/**< Copy symbol at runtime. */
#define ELF_R_X86_64_GLOB_DAT	6		/**< Create GOT entry. */
#define ELF_R_X86_64_JUMP_SLOT	7		/**< Create PLT entry. */
#define ELF_R_X86_64_RELATIVE	8		/**< Adjust by program base. */
#define ELF_R_X86_64_GOTPCREL	9		/**< 32-bit signed PC relative offset to GOT. */
#define ELF_R_X86_64_32		10		/**< Direct 32-bit zero-extended. */
#define ELF_R_X86_64_32S	11		/**< Direct 32-bit sign-extended. */
#define ELF_R_X86_64_16		12		/**< Direct 16-bit zero-extended. */
#define ELF_R_X86_64_PC16	13		/**< 16-bit sign-extended PC relative. */
#define ELF_R_X86_64_8		14		/**< Direct 8-bit sign-extended. */
#define ELF_R_X86_64_PC8	15		/**< 8-bit sign-extended PC relative. */
#define ELF_R_X86_64_DPTMOD64	16		/**< ID of module containing symbol. */
#define ELF_R_X86_64_DTPOFF64	17		/**< Offset in module's TLS block. */
#define ELF_R_X86_64_TPOFF64	18		/**< Offset in initial TLS block. */
#define ELF_R_X86_64_TLSGD	19		/**< 32-bit signed PC relative offset to two GOT entries (GD). */
#define ELF_R_X86_64_TLSLD	20		/**< 32-bit signed PC relative offset to two GOT entries (LD). */
#define ELF_R_X86_64_DTPOFF32	21		/**< Offset in TLS block. */
#define ELF_R_X86_64_GOTTPOFF	22		/**< 32-bit signed PC relative offset to GOT entry (IE). */
#define ELF_R_X86_64_TPOFF32	23		/**< Offset in initial TLS block. */
#define ELF_R_X86_64_PC64	24
#define ELF_R_X86_64_GOTOFF64	25
#define ELF_R_X86_64_GOTPC32	26

#define ELF_ST_BIND(i)		((i)>>4)
#define ELF_ST_TYPE(i)		((i)&0xf)
#define ELF_ST_INFO(b,t)	(((b)<<4)+((t)&0xf))

/** Values for ELF_ST_BIND (symbol binding). */
#define ELF_STB_LOCAL		0		/**< Local symbol. */
#define ELF_STB_GLOBAL		1		/**< Global symbol. */
#define ELF_STB_WEAK		2		/**< Weak symbol. */
#define	ELF_STB_NUM		3		/**< Number of defined types. */
#define ELF_STB_LOOS		10		/**< Start of OS-specific. */
#define ELF_STB_HIOS		12		/**< End of OS-specific. */
#define ELF_STB_LOPROC		13		/**< Start of processor-specific. */
#define ELF_STB_HIPROC		15		/**< End of processor-specific. */

/** Values for ELF_ST_TYPE (symbol type). */
#define ELF_STT_NOTYPE		0		/**< Symbol type is unspecified. */
#define ELF_STT_OBJECT		1		/**< Symbol is a data object. */
#define ELF_STT_FUNC		2		/**< Symbol is a code object. */
#define ELF_STT_SECTION		3		/**< Symbol associated with a section. */
#define ELF_STT_FILE		4		/**< Symbol's name is file name. */
#define ELF_STT_COMMON		5		/**< Symbol is a common data object. */
#define ELF_STT_TLS		6		/**< Symbol is thread-local data object. */
#define	ELF_STT_NUM		7		/**< Number of defined types. */
#define ELF_STT_LOOS		10		/**< Start of OS-specific. */
#define ELF_STT_HIOS		12		/**< End of OS-specific. */
#define ELF_STT_LOPROC		13		/**< Start of processor-specific. */
#define ELF_STT_HIPROC		15		/**< End of processor-specific. */

/** Special symbol table indices. */
#define ELF_STN_UNDEF		0		/**< End of a chain. */

#define ELF32_R_SYM(i)		((i)>>8)
#define ELF32_R_TYPE(i)		((unsigned char)(i))
#define ELF32_R_INFO(s,t)	(((s)<<8)+(unsigned char)(t))

typedef struct {
	unsigned char e_ident[16];		/**< ELF identiﬁcation. */
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

typedef struct {
	Elf32_Word    st_name;			/**< Symbol name. */
	Elf32_Addr    st_value;			/**< Symbol value. */
	Elf32_Word    st_size;			/**< Size of object (e.g., common). */
	unsigned char st_info;			/**< Type and Binding attributes. */
	unsigned char st_other;			/**< Reserved. */
	Elf32_Half    st_shndx;			/**< Section table index. */
} __packed Elf32_Sym;

typedef struct {
	Elf32_Addr    r_offset;			/**< Address of reference. */
	Elf32_Word    r_info;			/**< Symbol index and type of relocation. */
} __packed Elf32_Rel;

typedef struct {
	Elf32_Addr    r_offset;			/**< Address of reference. */
	Elf32_Word    r_info;			/**< Symbol index and type of relocation. */
	Elf32_Sword   r_addend;			/**< Constant part of expression. */
} __packed Elf32_Rela;

typedef struct {
	Elf32_Sword d_tag;
	union {
		Elf32_Word d_val;
		Elf32_Addr d_ptr;
	} d_un;
} __packed Elf32_Dyn;

typedef struct {
	Elf32_Word n_namesz;			/**< Length of the note's name. */
	Elf32_Word n_descsz;			/**< Length of the note's descriptor. */
	Elf32_Word n_type;			/**< Type of the note. */
} __packed Elf32_Note;

#define ELF64_R_SYM(i)		((i) >> 32)
#define ELF64_R_TYPE(i)		((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t)	((((Elf64_Xword)(s)) << 32) + ((t) & 0xffffffffL))

typedef struct {
	unsigned char e_ident[16];		/**< ELF identiﬁcation. */
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

typedef struct {
	Elf64_Word    st_name;			/**< Symbol name. */
	unsigned char st_info;			/**< Type and Binding attributes. */
	unsigned char st_other;			/**< Reserved. */
	Elf64_Half    st_shndx;			/**< Section table index. */
	Elf64_Addr    st_value;			/**< Symbol value. */
	Elf64_Xword   st_size;			/**< Size of object (e.g., common). */
} __packed Elf64_Sym;

typedef struct {
	Elf64_Addr   r_offset;			/**< Address of reference. */
	Elf64_Xword  r_info;			/**< Symbol index and type of relocation. */
} __packed Elf64_Rel;

typedef struct {
	Elf64_Addr   r_offset;			/**< Address of reference. */
	Elf64_Xword  r_info;			/**< Symbol index and type of relocation. */
	Elf64_Sxword r_addend;			/**< Constant part of expression. */
} __packed Elf64_Rela;

typedef struct {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword  d_val;
		Elf64_Addr   d_ptr;
	} d_un;
} __packed Elf64_Dyn;

typedef struct {
	Elf64_Word n_namesz;			/**< Length of the note's name. */
	Elf64_Word n_descsz;			/**< Length of the note's descriptor. */
	Elf64_Word n_type;			/**< Type of the note. */
} __packed Elf64_Note;

#include <arch/elf.h>

struct module;
struct object_handle;
struct vm_aspace;

extern bool elf_binary_check(struct object_handle *handle);
extern int elf_binary_load(struct object_handle *handle, struct vm_aspace *as, void **datap);
extern ptr_t elf_binary_finish(void *data);
extern void elf_binary_cleanup(void *data);

extern bool elf_module_check(struct object_handle *handle);
extern int elf_module_load(struct module *module);
extern int elf_module_relocate(struct module *module, bool external);

#endif /* __ELF_H */
