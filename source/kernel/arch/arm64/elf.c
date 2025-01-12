/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 ELF helper functions.
 *
 * Reference:
 *  - ELF for the Arm 64-bit Architecture (AArch64)
 *    https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst
 */

#include <assert.h>
#include <elf.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

enum {
    INSN_TYPE_B,
    INSN_TYPE_ADD,
    INSN_TYPE_ADR,
    INSN_TYPE_LDST12,
};

static status_t reloc_instruction(
    uint32_t *P32, int64_t val, uint32_t val_shift, uint32_t val_bits,
    uint32_t insn_type, bool check_overflow)
{
    if (check_overflow && (val & ((1 << val_shift) - 1)))
        return STATUS_OVERFLOW;

    val >>= val_shift;

    if (check_overflow && (val >= (int64_t)(1ul << (val_bits - 1)) || val < -((int64_t)(1ul << (val_bits - 1))))) {
        // TODO: Can support this via PLT?
        return STATUS_OVERFLOW;
    }

    val &= (1 << val_bits) - 1;

    uint32_t insn = *P32;

    switch (insn_type) {
        case INSN_TYPE_B:
            insn &= ~0x3ffffffu;
            insn |= val;
            break;
        case INSN_TYPE_ADD:
        case INSN_TYPE_LDST12:
            insn &= ~0x3ffc00u;
            insn |= val << 10;
            break;
        case INSN_TYPE_ADR:
            insn &= ~0x60ffffe0u;
            insn |= ((val & 0x3) << 29) | ((val >> 2) << 5);
            break;
        default:
            assert(false);
    }

    *P32 = insn;

    return STATUS_SUCCESS;
}

static inline uint64_t Page(uint64_t val) {
    return val & ~(uint64_t)0xfff;
}

/** Perform a RELA relocation on an ELF module. */
status_t arch_elf_module_relocate_rela(elf_image_t *image, elf_rela_t *rel, elf_shdr_t *target) {
    status_t ret;

    /*
     * Variable names match the relocation operation values in the Arm
     * documentation.
     */

    uint64_t A    = rel->r_addend;
    uint64_t P    = target->sh_addr + rel->r_offset;
    uint64_t *P64 = (uint64_t *)P;
    uint32_t *P32 = (uint32_t *)P;

    /* Obtain the symbol value. */
    uint64_t S;
    ret = elf_module_resolve(image, ELF64_R_SYM(rel->r_info), &S);
    if (ret != STATUS_SUCCESS)
        return ret;

    uint64_t O;

    /* Perform the relocation. */
    ret = STATUS_SUCCESS;
    switch (ELF64_R_TYPE(rel->r_info)) {
        case ELF_R_AARCH64_NONE:
            break;
        case ELF_R_AARCH64_ABS64:
            O = S + A;
            *P64 = O;
            break;
        case ELF_R_AARCH64_ADD_ABS_LO12_NC:
            /* IMM field in ADD instruction. */
            O = S + A;
            ret = reloc_instruction(P32, O, 0, 12, INSN_TYPE_ADD, false);
            break;
        case ELF_R_AARCH64_ADR_PREL_PG_HI21:
            O = Page(S + A) - Page(P);
            ret = reloc_instruction(P32, O, 12, 21, INSN_TYPE_ADR, true);
            break;
        case ELF_R_AARCH64_CALL26:
        case ELF_R_AARCH64_JUMP26:
            /* IMM field in B/BL instructions. */
            O = S + A - P;
            ret = reloc_instruction(P32, O, 2, 26, INSN_TYPE_B, true);
            break;
        case ELF_R_AARCH64_LDST16_ABS_LO12_NC:
            O = S + A;
            ret = reloc_instruction(P32, O, 1, 11, INSN_TYPE_LDST12, false);
            break;
        case ELF_R_AARCH64_LDST32_ABS_LO12_NC:
            O = S + A;
            ret = reloc_instruction(P32, O, 2, 10, INSN_TYPE_LDST12, false);
            break;
        case ELF_R_AARCH64_LDST64_ABS_LO12_NC:
            O = S + A;
            ret = reloc_instruction(P32, O, 3, 9, INSN_TYPE_LDST12, false);
            break;
        default:
            kprintf(LOG_WARN, "elf: encountered unknown relocation type: %lu\n", ELF64_R_TYPE(rel->r_info));
            return STATUS_MALFORMED_IMAGE;
    }

    return ret;
}

/** Perform a REL relocation on an ELF module. */
status_t arch_elf_module_relocate_rel(elf_image_t *image, elf_rel_t *rel, elf_shdr_t *target) {
    kprintf(LOG_WARN, "elf: REL relocation section unsupported\n");
    return STATUS_NOT_IMPLEMENTED;
}
