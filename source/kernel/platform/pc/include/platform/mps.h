/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Multiprocessor Specification structures.
 */

#ifndef __PLATFORM_MPS_H
#define __PLATFORM_MPS_H

#include <types.h>

/** MP Floating Pointer structure. */
typedef struct mp_fptr {
	char     signature[4];			/**< Signature. */
	uint32_t phys_addr_ptr;			/**< Physical address pointer. */
	uint8_t  length;			/**< Length. */
	uint8_t  spec_rev;			/**< Specification revision. */
	uint8_t  checksum;			/**< Checksum. */
	uint8_t  feature1;			/**< Feature 1. */
	uint8_t  feature2;			/**< Feature 2. */
	uint8_t  feature3;			/**< Feature 3. */
	uint8_t  feature4;			/**< Feature 4. */
	uint8_t  feature5;			/**< Feature 5. */
} __packed mp_fptr_t;

/** MP Configuration Table structure. */
typedef struct mp_config {
	char     signature[4];			/**< Signature. */
	uint16_t length;			/**< Length. */
	uint8_t  spec_rev;			/**< Specification revision. */
	uint8_t  checksum;			/**< Checksum. */
	char     oemid[8];			/**< OEM ID. */
	char     productid[12];			/**< Product ID. */
	uint32_t oem_table_ptr;			/**< OEM table pointer. */
	uint16_t oem_table_size;		/**< OEM table size. */
	uint16_t entry_count;			/**< Entry count. */
	uint32_t lapic_addr;			/**< LAPIC address. */
	uint16_t ext_tbl_len;			/**< Extended table length. */
	uint8_t  ext_tbl_checksum;		/**< Extended table checksum. */
	uint8_t  reserved;			/**< Reserved. */
} __packed mp_config_t;

/** MP CPU structure. */
typedef struct mp_cpu {
	uint8_t type;				/**< Type. */
	uint8_t lapic_id;			/**< LAPIC ID. */
	uint8_t lapic_version;			/**< LAPIC version. */
	uint8_t cpu_flags;			/**< Flags. */
	uint32_t signature;			/**< Signature. */
} __packed mp_cpu_t;

/** MP Configuration Table entry types. */
#define MP_CONFIG_CPU			0	/**< Processor. */
#define MP_CONFIG_BUS			1	/**< Bus. */
#define MP_CONFIG_IOAPIC		2	/**< I/O APIC. */
#define MP_CONFIG_IO_INTR		3	/**< I/O Interrupt Assignment. */
#define MP_CONFIG_LOCAL_INTR		4	/**< Local Interrupt Assignment. */

#endif /* __PLATFORM_MPS_H */
