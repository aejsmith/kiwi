/* Kiwi IA32 type definitions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		IA32 type definitions.
 */

#ifndef __ARCH_TYPES_H
#define __ARCH_TYPES_H

#include <compiler.h>

/** Format character definitions for kprintf(). */
#define PRIu8		"u"		/**< Format for uint8_t. */
#define PRIu16		"u"		/**< Format for uint16_t. */
#define PRIu32		"u"		/**< Format for uint32_t. */
#define PRIu64		"llu"		/**< Format for uint64_t. */
#define PRIun		"lu"		/**< Format for unative_t. */
#define PRId8		"d"		/**< Format for int8_t. */
#define PRId16		"d"		/**< Format for int16_t. */
#define PRId32		"d"		/**< Format for int32_t. */
#define PRId64		"lld"		/**< Format for int64_t. */
#define PRIdn		"d"		/**< Format for native_t. */
#define PRIx8		"x"		/**< Format for (u)int8_t (hexadecimal). */
#define PRIx16		"x"		/**< Format for (u)int16_t (hexadecimal). */
#define PRIx32		"x"		/**< Format for (u)int32_t (hexadecimal). */
#define PRIx64		"llx"		/**< Format for (u)int64_t (hexadecimal). */
#define PRIxn		"lx"		/**< Format for (u)native_t (hexadecimal). */
#define PRIo8		"o"		/**< Format for (u)int8_t (octal). */
#define PRIo16		"o"		/**< Format for (u)int16_t (octal). */
#define PRIo32		"o"		/**< Format for (u)int32_t (octal). */
#define PRIo64		"llo"		/**< Format for (u)int64_t (octal). */
#define PRIon		"lo"		/**< Format for (u)native_t (octal). */
#define PRIpp		"llx"		/**< Format for phys_ptr_t. */

/** Unsigned data types. */
typedef unsigned char uint8_t;		/**< Unsigned 8-bit. */
typedef unsigned short uint16_t;	/**< Unsigned 16-bit. */
typedef unsigned int uint32_t;		/**< Unsigned 32-bit. */
typedef unsigned long long uint64_t;	/**< Unsigned 64-bit. */

/** Signed data types. */
typedef signed char int8_t;		/**< Signed 8-bit. */
typedef signed short int16_t;		/**< Signed 16-bit. */
typedef signed int int32_t;		/**< Signed 32-bit. */
typedef signed long long int64_t;	/**< Signed 64-bit. */

/** Native-sized types. */
typedef unsigned long unative_t;	/**< Unsigned native-sized type. */
typedef signed long native_t;		/**< Signed native-sized type. */

/** Integer type that can represent a virtual address. */
typedef unsigned long ptr_t;

/** Integer type that can represent a physical address. */
typedef uint64_t phys_ptr_t;

#endif /* __ARCH_TYPES_H */
