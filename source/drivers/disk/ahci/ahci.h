/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		AHCI structures/definitions.
 *
 * Reference:
 * - Serial ATA AHCI 1.3 Specification
 *   http://www.intel.com/technology/serialata/ahci.htm
 */

#ifndef __AHCI_H
#define __AHCI_H

#include <drivers/ata.h>
#include <drivers/pci.h>

#include <io/device.h>

#include <mm/page.h>

#include <time.h>

/** Bits in the HBA Capabilities register. */
#define AHCI_CAP_NP_MASK	0x1F		/**< Number of Ports mask. */
#define AHCI_CAP_NP_SHIFT	0		/**< Number of Ports shift. */
#define AHCI_CAP_SXS		(1<<5)		/**< Supports External SATA. */
#define AHCI_CAP_EMS		(1<<6)		/**< Enclosure Management Supported. */
#define AHCI_CAP_CCCS		(1<<7)		/**< Command Completion Coalescing Supported. */
#define AHCI_CAP_NCS_MASK	0x1F00		/**< Number of Command Slots mask. */
#define AHCI_CAP_NCS_SHIFT	8		/**< Number of Command Slots shift. */
#define AHCI_CAP_PSC		(1<<13)		/**< Partial State Capable. */
#define AHCI_CAP_SSC		(1<<14)		/**< Slumber State Capable. */
#define AHCI_CAP_PMD		(1<<15)		/**< PIO Multiple DRQ Block. */
#define AHCI_CAP_FBSS		(1<<16)		/**< FIS-based Switching Supported. */
#define AHCI_CAP_SPM		(1<<17)		/**< Supports Port Multiplier. */
#define AHCI_CAP_SAM		(1<<18)		/**< Supports AHCI mode only. */
#define AHCI_CAP_ISS_MASK	0xF00000	/**< Interface Speed Support mask. */
#define AHCI_CAP_ISS_SHIFT	20		/**< Interface Speed Support shift. */
#define AHCI_CAP_SCLO		(1<<24)		/**< Supports Command List Override. */
#define AHCI_CAP_SAL		(1<<25)		/**< Supports Activity LED. */
#define AHCI_CAP_SALP		(1<<26)		/**< Supports Aggressive Link Power Management. */
#define AHCI_CAP_SSS		(1<<27)		/**< Supports Staggered Spin-up. */
#define AHCI_CAP_SMPS		(1<<28)		/**< Supports Mechanical Presence Switch. */
#define AHCI_CAP_SSNTF		(1<<29)		/**< Supports SNotification Register. */
#define AHCI_CAP_SNCQ		(1<<30)		/**< Supports Native Command Queueing. */
#define AHCI_CAP_S64A		(1<<31)		/**< Supports 64-bit Addressing. */

/** Bits in the Global HBA Control register. */
#define AHCI_GHC_HR		(1<<0)		/**< HBA Reset. */
#define AHCI_GHC_IE		(1<<1)		/**< Interrupt Enable. */
#define AHCI_GHC_MRSM		(1<<2)		/**< MSI Revert to Single Message. */
#define AHCI_GHC_AE		(1<<31)		/**< AHCI Enable. */

/** Bits in the Port x Interrupt Status register. */
#define AHCI_PXIS_DHRS		(1<<0)		/**< Device to Host Register. */
#define AHCI_PXIS_PSS		(1<<1)		/**< PIO Setup FIS. */
#define AHCI_PXIS_DSS		(1<<2)		/**< DMA Setup FIS. */
#define AHCI_PXIS_SDBS		(1<<3)		/**< Set Device Bits. */
#define AHCI_PXIS_UFS		(1<<4)		/**< Unknown FIS. */
#define AHCI_PXIS_DPS		(1<<5)		/**< Descriptor Processed. */
#define AHCI_PXIS_PCS		(1<<6)		/**< Port Connect Change Status. */
#define AHCI_PXIS_DMPS		(1<<7)		/**< Device Mechanical Presence Status. */
#define AHCI_PXIS_PRCS		(1<<22)		/**< PhyRdy Change Status. */
#define AHCI_PXIS_IPMS		(1<<23)		/**< Incorrect Port Multiplier Status. */
#define AHCI_PXIS_OFS		(1<<24)		/**< Overflow Status. */
#define AHCI_PXIS_INFS		(1<<26)		/**< Interface Non-Fatal Error Status. */
#define AHCI_PXIS_IFS		(1<<27)		/**< Interface Fatal Error Status. */
#define AHCI_PXIS_HBDS		(1<<28)		/**< Host Bus Data Error Status. */
#define AHCI_PXIS_HBFS		(1<<29)		/**< Host Bus Fatal Error Status. */
#define AHCI_PXIS_TFES		(1<<30)		/**< Task File Error Status. */
#define AHCI_PXIS_CPDS		(1<<31)		/**< Cold Port Detect Status. */

/** Bits in the Port x Interrupt Enable register. */
#define AHCI_PXIE_DHRE		(1<<0)		/**< Device to Host Register Enable. */
#define AHCI_PXIE_PSE		(1<<1)		/**< PIO Setup FIS Enable. */
#define AHCI_PXIE_DSE		(1<<2)		/**< DMA Setup FIS Enable. */
#define AHCI_PXIE_SDBE		(1<<3)		/**< Set Device Bits Enable. */
#define AHCI_PXIE_UFE		(1<<4)		/**< Unknown FIS Enable. */
#define AHCI_PXIE_DPE		(1<<5)		/**< Descriptor Processed Enable. */
#define AHCI_PXIE_PCE		(1<<6)		/**< Port Connect Change Enable. */
#define AHCI_PXIE_DMPE		(1<<7)		/**< Device Mechanical Presence Enable. */
#define AHCI_PXIE_PRCE		(1<<22)		/**< PhyRdy Change Enable. */
#define AHCI_PXIE_IPME		(1<<23)		/**< Incorrect Port Multiplier Enable. */
#define AHCI_PXIE_OFE		(1<<24)		/**< Overflow Enable. */
#define AHCI_PXIE_INFE		(1<<26)		/**< Interface Non-Fatal Error Enable. */
#define AHCI_PXIE_IFE		(1<<27)		/**< Interface Fatal Error Enable. */
#define AHCI_PXIE_HBDE		(1<<28)		/**< Host Bus Data Error Enable. */
#define AHCI_PXIE_HBFE		(1<<29)		/**< Host Bus Fatal Error Enable. */
#define AHCI_PXIE_TFEE		(1<<30)		/**< Task File Error Enable. */
#define AHCI_PXIE_CPDE		(1<<31)		/**< Cold Port Detect Enable. */

/** Error interrupts to enable. */
#define AHCI_PORT_INTR_ERROR	\
	(AHCI_PXIE_UFE | AHCI_PXIE_PCE | AHCI_PXIE_PRCE | AHCI_PXIE_IPME | \
	 AHCI_PXIE_OFE | AHCI_PXIE_INFE | AHCI_PXIE_IFE | AHCI_PXIE_HBDE | \
	 AHCI_PXIE_HBFE | AHCI_PXIE_TFEE)

/** Bits in the Port x Command and Status register. */
#define AHCI_PXCMD_ST		(1<<0)		/**< Start. */
#define AHCI_PXCMD_SUD		(1<<1)		/**< Spin-Up Device. */
#define AHCI_PXCMD_POD		(1<<2)		/**< Power On Device. */
#define AHCI_PXCMD_CLO		(1<<3)		/**< Command List Override. */
#define AHCI_PXCMD_FRE		(1<<4)		/**< FIS Receive Enable. */
#define AHCI_PXCMD_CCS_MASK	0x1F00		/**< Current Command Slot mask. */
#define AHCI_PXCMD_CCS_SHIFT	8		/**< Current Command Slot shift. */
#define AHCI_PXCMD_MPSS		(1<<13)		/**< Mechanical Presence Switch State. */
#define AHCI_PXCMD_FR		(1<<14)		/**< FIS Receive Running. */
#define AHCI_PXCMD_CR		(1<<15)		/**< Command List Running. */
#define AHCI_PXCMD_CPS		(1<<16)		/**< Cold Presence State. */
#define AHCI_PXCMD_PMA		(1<<17)		/**< Port Multiplier Attached. */
#define AHCI_PXCMD_HPCP		(1<<18)		/**< Hot Plug Capable Port. */
#define AHCI_PXCMD_MPSP		(1<<19)		/**< Mechanical Presence Switch Attached to Port. */
#define AHCI_PXCMD_CPD		(1<<20)		/**< Cold Presence Detection. */
#define AHCI_PXCMD_ESP		(1<<21)		/**< External SATA Port. */
#define AHCI_PXCMD_FBSCP	(1<<22)		/**< FIS-Based Switching Capable Port. */
#define AHCI_PXCMD_APSTE	(1<<23)		/**< Automatic Partial to Slumber Transitions Enabled. */
#define AHCI_PXCMD_ATAPI	(1<<24)		/**< Device is ATAPI. */
#define AHCI_PXCMD_DLAE		(1<<25)		/**< Device LED on ATAPI Enable. */
#define AHCI_PXCMD_ALPE		(1<<26)		/**< Aggressive Link Power Management Enable. */
#define AHCI_PXCMD_ASP		(1<<27)		/**< Aggressive Slumber/Partial. */
#define AHCI_PXCMD_ICC_MASK	0xF0000000	/**< Interface Communication Control mask. */
#define AHCI_PXCMD_ICC_SHIFT	28		/**< Interface Communication Control shift. */

struct ahci_port;

/** AHCI Received FIS Structure. */
typedef struct ahci_fis {
	uint8_t dsfis[0x1C];			/**< DMA Setup FIS. */
	uint8_t reserved1[0x04];		/**< Reserved. */
	uint8_t psfis[0x14];			/**< PIO Setup FIS. */
	uint8_t reserved2[0x0C];		/**< Reserved. */
	uint8_t rfis[0x14];			/**< D2H Register FIS. */
	uint8_t reserved3[0x04];		/**< Reserved. */
	uint8_t sdbfis[0x08];			/**< Set Device Bits FIS. */
	uint8_t ufis[0x40];			/**< Unknown FIS. */
	uint8_t reserved4[0x60];		/**< Reserved. */
} __packed ahci_fis_t;

/** AHCI Command Header structure. */
typedef struct ahci_command_header {
	/** DW0 - Description Information. */
	union {
		struct {
			uint16_t cfl : 5;	/**< Command FIS Length. */
			uint16_t a : 1;		/**< ATAPI. */
			uint16_t w : 1;		/**< Write. */
			uint16_t p : 1;		/**< Prefetchable. */
			uint16_t r : 1;		/**< Reset. */
			uint16_t b : 1;		/**< BIST. */
			uint16_t c : 1;		/**< Clear Busy upon R_OK. */
			uint16_t reserved1 : 1;	/**< Reserved. */
			uint16_t pmp : 4;	/**< Port Multiplier Port. */
			uint16_t prdtl;		/**< Physical Region Descriptor Table Length. */
		} __packed;
		uint32_t dw0;
	} __packed;

	/** DW1 - Command Status. */
	uint32_t prdbc;				/**< Physical Region Descriptor Byte Count. */

	/** DW2 - Command Table Base Address.
	 * @note		Bits 0-6 are reserved, must be 0. */
	uint32_t ctba;				/**< Command Table Descriptor Base Address. */

	/** DW3 - Command Table Base Address Upper. */
	uint32_t ctbau;				/**< Command Table Descriptor Base Address Upper 32-bits. */

	/** DW4-7 - Reserved. */
	uint32_t reserved2[4];
} __packed ahci_command_header_t;

/** AHCI Command Table.
 * @note		This structure should be immediately followed by the
 *			PRDT. */
typedef struct ahci_command_table {
	/** Command FIS - Host to Device. */
	struct {
		uint8_t type;			/**< FIS Type (0x27). */
		uint8_t pm_port : 4;		/**< Port Multiplier Port. */
		uint8_t reserved1 : 3;		/**< Reserved. */
		uint8_t c_bit : 1;		/**< C bit. */
		uint8_t command;		/**< ATA Command. */
		uint8_t features_0_7;		/**< Features (bits 0-7). */
		uint8_t lba_0_7;		/**< LBA (bits 0-7). */
		uint8_t lba_8_15;		/**< LBA (bits 8-15). */
		uint8_t lba_16_23;		/**< LBA (bits 16-23). */
		uint8_t device;			/**< Device (bits 24-27 for LBA28). */
		uint8_t lba_24_31;		/**< LBA48 (bits 24-31). */
		uint8_t lba_32_39;		/**< LBA48 (bits 32-39). */
		uint8_t lba_40_47;		/**< LBA48 (bits 40-47). */
		uint8_t features_8_15;		/**< Features (bits 8-15). */
		uint8_t count_0_7;		/**< Sector Count (bits 0-7). */
		uint8_t count_8_15;		/**< Sector Count (bits 8-15). */
		uint8_t icc;			/**< Isochronous Command Completion. */
		uint8_t control;		/**< Device Control. */
		uint32_t reserved2;		/**< Reserved. */
		uint8_t padding[0x2C];		/**< Padding to 64 bytes. */
	} __packed cfis;

	uint8_t acmd[0x10];			/**< ATAPI Command (12 or 16 bytes). */
	uint8_t reserved[0x30];			/**< Reserved. */
} __packed ahci_command_table_t;

/** AHCI Physical Region Descriptor. */
typedef struct ahci_prd {
	/** DW0 - Data Base Address. */
	uint32_t dba;				/**< Data Base Address. */

	/** DW1 - Data Base Address Upper. */
	uint32_t dbau;				/**< Data Base Address Upper 32-bits. */

	/** DW2 - Reserved */
	uint32_t reserved1;			/**< Reserved. */

	/** DW3 - Description Information. */
	union {
		struct {
			uint32_t dbc : 22;	/**< Data Byte Count. */
			uint32_t reserved2 : 9;	/**< Reserved. */
			uint32_t i : 1;		/**< Interrupt on Completion. */
		} __packed;
		uint32_t dw3;
	} __packed;
} __packed ahci_prd_t;

/** Structure containing AHCI port registers. */
typedef struct ahci_port_regs {
	uint32_t clb;				/**< Command List Base Address. */
	uint32_t clbu;				/**< Command List Base Address Upper 32-Bits. */
	uint32_t fb;				/**< FIS Base Address. */
	uint32_t fbu;				/**< FIS Base Address Upper 32-Bits. */
	uint32_t is;				/**< Interrupt Status. */
	uint32_t ie;				/**< Interrupt Enable. */
	uint32_t cmd;				/**< Command and Status. */
	uint32_t reserved1;			/**< Reserved. */

	/** Task File Data. */
	union {
		struct {
			uint8_t status;		/**< Status Register. */
			uint8_t err;		/**< Error Register. */
			uint16_t reserved;	/**< Reserved. */
		} __packed tfd;
		uint32_t _tfd;
	} __packed;

	uint32_t sig;				/**< Signature. */
	uint32_t ssts;				/**< Serial ATA Status (SCR0: SStatus). */
	uint32_t sctl;				/**< Serial ATA Control (SCR2: SControl). */
	uint32_t serr;				/**< Serial ATA Error (SCR1: SError). */
	uint32_t sact;				/**< Serial ATA Active (SCR3: SActive). */
	uint32_t ci;				/**< Command Issue. */
	uint32_t sntf;				/**< Serial ATA Notification (SCR4: SNotification). */
	uint32_t fbs;				/**< FIS-based Switching Control. */
	uint32_t reserved2[11];			/**< Reserved. */
	uint32_t vs[4];				/**< Vendor Specific. */
} __packed ahci_port_regs_t;

/** Structure containing AHCI HBA registers. */
typedef struct ahci_hba_regs {
	uint32_t cap;				/**< Host Capabilities. */
	uint32_t ghc;				/**< Global Host Control. */
	uint32_t is;				/**< Interrupt Status. */
	uint32_t pi;				/**< Ports Implemented. */
	uint32_t vs;				/**< Version. */
	uint32_t ccc_ctl;			/**< Command Completion Coalescing Control. */
	uint32_t ccc_ports;			/**< Command Completion Coalescing Ports. */
	uint32_t em_loc;			/**< Enclosure Management Location. */
	uint32_t em_ctl;			/**< Enclosure Management Control. */
	uint32_t cap2;				/**< Host Capabilities Extended. */
	uint32_t bohc;				/**< BIOS/OS Handoff Control and Status. */
	uint32_t reserved[29];			/**< Reserved/Reserved for NVMHCI. */
	uint32_t vendor[24];			/**< Vendor Specific registers. */
	ahci_port_regs_t ports[32];		/**< Port registers. */
} __packed ahci_hba_regs_t;

/** AHCI HBA information structure. */
typedef struct ahci_hba {
	int id;					/**< ID of the HBA. */
	pci_device_t *pci_device;		/**< PCI device that the HBA is on. */
	volatile ahci_hba_regs_t *regs;		/**< Mapped registers for the port. */
	uint32_t irq;				/**< IRQ for the HBA. */
	device_t *node;				/**< Device tree node for the HBA. */
	struct ahci_port *ports[32];		/**< Pointers to available ports. */
} ahci_hba_t;

/** AHCI port information structure. */
typedef struct ahci_port {
	uint8_t num;				/**< Number of the port. */
	ahci_hba_t *parent;			/**< HBA that the port is on. */
	device_t *node;				/**< Device tree node. */
	ata_channel_t *channel;			/**< ATA channel for the port. */
	bool present;				/**< Whether a device is present. */
	phys_ptr_t mem_phys;			/**< Physical address of the port memory. */
	volatile void *mem_virt;		/**< Virtual address of the port memory. */
	bool error;				/**< Whether an error was detected during DMA. */
	bool reset;				/**< Whether the error requires a reset. */

	volatile ahci_port_regs_t *regs;	/**< Registers for this port. */
	volatile ahci_fis_t *fis;		/**< Received FIS structure. */
	volatile ahci_command_header_t *clist;	/**< Command List. */
	volatile ahci_command_table_t *ctbl;	/**< Command Table structure. */
	volatile ahci_prd_t *prdt;		/**< Physical Region Descriptor Table. */
} ahci_port_t;

/** Amount of memory to allocate for a port's structures. */
#define AHCI_PORT_MEM_SIZE		PAGE_SIZE

/** Number of command headers. */
#define AHCI_COMMAND_HEADER_COUNT	32

/** Number of PRDT entries. */
#define AHCI_PRD_COUNT			\
	((AHCI_PORT_MEM_SIZE - sizeof(ahci_fis_t) - sizeof(ahci_command_table_t) - \
	(sizeof(ahci_command_header_t) * AHCI_COMMAND_HEADER_COUNT)) / sizeof(ahci_prd_t))

/** Wait for bits to become clear.
 * @param reg		Register to wait on.
 * @param bits		Bits to wait for.
 * @param any		Whether to wait for any or all of the bits to be clear.
 * @param timeout	Maximum time to wait.
 * @return		True if succeeded, false if timed out. */
static inline bool wait_for_clear(volatile uint32_t *reg, uint32_t bits, bool any, useconds_t timeout) {
	useconds_t i;

	while(timeout) {
		if(!(*reg & bits) || (any && (*reg & bits) != bits)) {
			return true;
		}
		i = (timeout < 1000) ? timeout : 1000;
		usleep(i);
		timeout -= i;
	}
	return false;
}

/** Wait for bits to become set.
 * @param reg		Register to wait on.
 * @param bits		Bits to wait for.
 * @param any		Whether to wait for any or all of the bits to be set.
 * @param timeout	Maximum time to wait.
 * @return		True if succeeded, false if timed out. */
static inline bool wait_for_set(volatile uint32_t *reg, uint32_t bits, bool any, useconds_t timeout) {
	useconds_t i;

	while(timeout) {
		if((*reg & bits) == bits || (any && (*reg & bits))) {
			return true;
		}
		i = (timeout < 1000) ? timeout : 1000;
		usleep(i);
		timeout -= i;
	}
	return false;
}

/** Flush writes to a HBA's registers.
 * @param hba		HBA to flush. */
static inline void ahci_hba_flush(ahci_hba_t *hba) {
	volatile uint32_t val = hba->regs->ghc;
	val = val;
}

extern bool ahci_hba_add(pci_device_t *device, void *data);

/** Flush writes to a port's registers.
 * @param hba		Port to flush. */
static inline void ahci_port_flush(ahci_port_t *port) {
	volatile uint32_t val = port->regs->cmd;
	val = val;
}

extern ahci_port_t *ahci_port_add(ahci_hba_t *hba, uint8_t num);
extern bool ahci_port_init(ahci_port_t *port);
extern void ahci_port_destroy(ahci_port_t *port);
extern status_t ahci_port_reset(ahci_port_t *port);
extern void ahci_port_interrupt(ahci_port_t *port);

#endif /* __AHCI_H */
