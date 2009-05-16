/* Kiwi ACPI structures/definitions
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
 * @brief		ACPI structures/definitions.
 */

#ifndef __PLATFORM_ACPI_H
#define __PLATFORM_ACPI_H

#include <compiler.h>
#include <types.h>

/** Signature definitions. */
#define ACPI_RSDP_SIGNATURE	"RSD PTR "	/**< RSDP signature. */
#define ACPI_MADT_SIGNATURE	"APIC"		/**< MADT signature. */
#define ACPI_DSDT_SIGNATURE	"DSDT"		/**< DSDT signature. */
#define ACPI_ECDT_SIGNATURE	"ECDT"		/**< ECDT signature. */
#define ACPI_FADT_SIGNATURE	"FACP"		/**< FADT signature. */
#define ACPI_FACS_SIGNATURE	"FACS"		/**< FACS signature. */
#define ACPI_PSDT_SIGNATURE	"PSDT"		/**< PSDT signature. */
#define ACPI_RSDT_SIGNATURE	"RSDT"		/**< RSDT signature. */
#define ACPI_SBST_SIGNATURE	"SBST"		/**< SBST signature. */
#define ACPI_SLIT_SIGNATURE	"SLIT"		/**< SLIT signature. */
#define ACPI_SRAT_SIGNATURE	"SRAT"		/**< SRAT signature. */
#define ACPI_SSDT_SIGNATURE	"SSDT"		/**< SSDT signature. */
#define ACPI_XSDT_SIGNATURE	"XSDT"		/**< XSDT signature. */

/** MADT APIC types. */
#define ACPI_MADT_LAPIC		0		/**< Processor Local APIC. */
#define ACPI_MADT_IOAPIC	1		/**< I/O APIC. */

/** Root System Description Pointer (RSDP) structure. */
typedef struct acpi_rsdp {
	uint8_t signature[8];			/**< Signature (ACPI_RSDP_SIGNATURE). */
	uint8_t checksum;			/**< Checksum of first 20 bytes. */
	uint8_t oem_id[6];			/**< OEM ID string. */
	uint8_t revision;			/**< ACPI revision number. */
	uint32_t rsdt_address;			/**< Address of RSDT. */
	uint32_t length;			/**< Length of RSDT in bytes. */
	uint64_t xsdt_address;			/**< Address of XSDT. */
	uint8_t ext_checksum;			/**< Checksum of entire table. */
	uint8_t reserved[3];			/**< Reserved field. */
} __packed acpi_rsdp_t;

/** System Description Table Header (DESCRIPTION_HEADER). */
typedef struct acpi_header {
	uint8_t signature[4];			/**< Signature. */
	uint32_t length;			/**< Length of header. */
	uint8_t revision;			/**< ACPI revision number. */
	uint8_t checksum;			/**< Checksum of the table. */
	uint8_t oem_id[6];			/**< OEM ID string. */
	uint8_t oem_table_id[8];		/**< OEM Table ID string. */
	uint32_t oem_revision;			/**< OEM Revision. */
	uint32_t creator_id;			/**< Creator ID. */
	uint32_t creator_revision;		/**< Creator Revision. */
} __packed acpi_header_t;

/** Root System Description Table (RSDT) structure. */
typedef struct acpi_rsdt {
	acpi_header_t header;			/**< ACPI Header. */
	uint32_t entry[];			/**< Array of entries. */
} __packed acpi_rsdt_t;

/** Extended System Description Table (XSDT) structure. */
typedef struct acpi_xsdt {
	acpi_header_t header;			/**< ACPI Header. */
	uint64_t entry[];			/**< Array of entries. */
} __packed acpi_xsdt_t;

/** Multiple APIC Description Table (MADT) structure. */
typedef struct acpi_madt {
	acpi_header_t header;			/**< ACPI Header. */
	uint32_t lapic_addr;			/**< Local APIC address. */
	uint32_t flags;				/**< Multiple APIC flags. */
	uint8_t apic_structures[];		/**< Array of APIC structures. */
} __packed acpi_madt_t;

/** MADT Processor Local APIC structure. */
typedef struct acpi_madt_lapic {
	uint8_t type;				/**< APIC type (0). */
	uint8_t length;				/**< Structure length. */
	uint8_t processor_id;			/**< APCI Processor ID. */
	uint8_t lapic_id;			/**< Processor's LAPIC ID. */
	uint32_t flags;				/**< LAPIC flags. */
} __packed acpi_madt_lapic_t;

extern bool acpi_supported;

extern acpi_header_t *acpi_table_find(const char *signature);
extern void acpi_init(void);

#endif /* __PLATFORM_ACPI_H */
