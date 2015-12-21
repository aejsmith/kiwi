/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief               Fixed size integer types.
 */

#ifndef __INTTYPES_H
#define __INTTYPES_H

#include <system/arch/types.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Type returned by imaxdiv(). */
typedef struct {
    intmax_t quot;                  /**< Quotient. */
    intmax_t rem;                   /**< Remainder. */
} imaxdiv_t;

/** Helper definitions for the printf()/scanf() type macros. */
#define __PRI_8_MODIFIER        ""
#define __PRI_16_MODIFIER       ""
#define __PRI_32_MODIFIER       ""
#if __WORDSIZE == 64
#   define __PRI_64_MODIFIER    "l"
#else
#   define __PRI_64_MODIFIER    "ll"
#endif
#define __PRI_MAX_MODIFIER      __PRI_64_MODIFIER

/** Format string macros for printf() for int8_t/uint8_t. */
#define PRId8           __PRI_8_MODIFIER "d"
#define PRIi8           __PRI_8_MODIFIER "i"
#define PRIo8           __PRI_8_MODIFIER "o"
#define PRIu8           __PRI_8_MODIFIER "u"
#define PRIx8           __PRI_8_MODIFIER "x"
#define PRIX8           __PRI_8_MODIFIER "X"

/** Format string macros for printf() for int16_t/uint16_t. */
#define PRId16          __PRI_16_MODIFIER "d"
#define PRIi16          __PRI_16_MODIFIER "i"
#define PRIo16          __PRI_16_MODIFIER "o"
#define PRIu16          __PRI_16_MODIFIER "u"
#define PRIx16          __PRI_16_MODIFIER "x"
#define PRIX16          __PRI_16_MODIFIER "X"

/** Format string macros for printf() for int32_t/uint32_t. */
#define PRId32          __PRI_32_MODIFIER "d"
#define PRIi32          __PRI_32_MODIFIER "i"
#define PRIo32          __PRI_32_MODIFIER "o"
#define PRIu32          __PRI_32_MODIFIER "u"
#define PRIx32          __PRI_32_MODIFIER "x"
#define PRIX32          __PRI_32_MODIFIER "X"

/** Format string macros for printf() for int64_t/uint64_t. */
#define PRId64          __PRI_64_MODIFIER "d"
#define PRIi64          __PRI_64_MODIFIER "i"
#define PRIo64          __PRI_64_MODIFIER "o"
#define PRIu64          __PRI_64_MODIFIER "u"
#define PRIx64          __PRI_64_MODIFIER "x"
#define PRIX64          __PRI_64_MODIFIER "X"

/** Format string macros for printf() for int_least8_t/uint_least8_t. */
#define PRIdLEAST8      PRId8
#define PRIiLEAST8      PRIi8
#define PRIoLEAST8      PRIo8
#define PRIuLEAST8      PRIu8
#define PRIxLEAST8      PRIx8
#define PRIXLEAST8      PRIX8

/** Format string macros for printf() for int_least16_t/uint_least16_t. */
#define PRIdLEAST16     PRId16
#define PRIiLEAST16     PRIi16
#define PRIoLEAST16     PRIo16
#define PRIuLEAST16     PRIu16
#define PRIxLEAST16     PRIx16
#define PRIXLEAST16     PRIX16

/** Format string macros for printf() for int_least32_t/uint_least32_t. */
#define PRIdLEAST32     PRId32
#define PRIiLEAST32     PRIi32
#define PRIoLEAST32     PRIo32
#define PRIuLEAST32     PRIu32
#define PRIxLEAST32     PRIx32
#define PRIXLEAST32     PRIX32

/** Format string macros for printf() for int_least64_t/uint_least64_t. */
#define PRIdLEAST64     PRId64
#define PRIiLEAST64     PRIi64
#define PRIoLEAST64     PRIo64
#define PRIuLEAST64     PRIu64
#define PRIxLEAST64     PRIx64
#define PRIXLEAST64     PRIX64

/** Format string macros for printf() for int_fast8_t/uint_fast8_t. */
#define PRIdFAST8       PRId8
#define PRIiFAST8       PRIi8
#define PRIoFAST8       PRIo8
#define PRIuFAST8       PRIu8
#define PRIxFAST8       PRIx8
#define PRIXFAST8       PRIX8

/** Format string macros for printf() for int_fast16_t/uint_fast16_t. */
#define PRIdFAST16      PRId16
#define PRIiFAST16      PRIi16
#define PRIoFAST16      PRIo16
#define PRIuFAST16      PRIu16
#define PRIxFAST16      PRIx16
#define PRIXFAST16      PRIX16

/** Format string macros for printf() for int_fast32_t/uint_fast32_t. */
#define PRIdFAST32      PRId32
#define PRIiFAST32      PRIi32
#define PRIoFAST32      PRIo32
#define PRIuFAST32      PRIu32
#define PRIxFAST32      PRIx32
#define PRIXFAST32      PRIX32

/** Format string macros for printf() for int_fast64_t/uint_fast64_t. */
#define PRIdFAST64      PRId64
#define PRIiFAST64      PRIi64
#define PRIoFAST64      PRIo64
#define PRIuFAST64      PRIu64
#define PRIxFAST64      PRIx64
#define PRIXFAST64      PRIX64

/** Format string macros for printf() for intptr_t/uintptr_t. */
#define PRIdPTR         "ld"
#define PRIiPTR         "li"
#define PRIoPTR         "lo"
#define PRIuPTR         "lu"
#define PRIxPTR         "lx"
#define PRIXPTR         "lX"

/** Format string macros for printf() for intmax_t/uintmax_t. */
#define PRIdMAX         __PRI_MAX_MODIFIER "d"
#define PRIiMAX         __PRI_MAX_MODIFIER "i"
#define PRIoMAX         __PRI_MAX_MODIFIER "o"
#define PRIuMAX         __PRI_MAX_MODIFIER "u"
#define PRIxMAX         __PRI_MAX_MODIFIER "x"
#define PRIXMAX         __PRI_MAX_MODIFIER "X"

/** Format string macros for scanf() for int8_t/uint8_t. */
#define SCNd8           __PRI_8_MODIFIER "d"
#define SCNi8           __PRI_8_MODIFIER "i"
#define SCNo8           __PRI_8_MODIFIER "o"
#define SCNu8           __PRI_8_MODIFIER "u"
#define SCNx8           __PRI_8_MODIFIER "x"
#define SCNX8           __PRI_8_MODIFIER "X"

/** Format string macros for scanf() for int16_t/uint16_t. */
#define SCNd16          __PRI_16_MODIFIER "d"
#define SCNi16          __PRI_16_MODIFIER "i"
#define SCNo16          __PRI_16_MODIFIER "o"
#define SCNu16          __PRI_16_MODIFIER "u"
#define SCNx16          __PRI_16_MODIFIER "x"
#define SCNX16          __PRI_16_MODIFIER "X"

/** Format string macros for scanf() for int32_t/uint32_t. */
#define SCNd32          __PRI_32_MODIFIER "d"
#define SCNi32          __PRI_32_MODIFIER "i"
#define SCNo32          __PRI_32_MODIFIER "o"
#define SCNu32          __PRI_32_MODIFIER "u"
#define SCNx32          __PRI_32_MODIFIER "x"
#define SCNX32          __PRI_32_MODIFIER "X"

/** Format string macros for scanf() for int64_t/uint64_t. */
#define SCNd64          __PRI_64_MODIFIER "d"
#define SCNi64          __PRI_64_MODIFIER "i"
#define SCNo64          __PRI_64_MODIFIER "o"
#define SCNu64          __PRI_64_MODIFIER "u"
#define SCNx64          __PRI_64_MODIFIER "x"
#define SCNX64          __PRI_64_MODIFIER "X"

/** Format string macros for scanf() for int_least8_t/uint_least8_t. */
#define SCNdLEAST8      PRId8
#define SCNiLEAST8      PRIi8
#define SCNoLEAST8      PRIo8
#define SCNuLEAST8      PRIu8
#define SCNxLEAST8      PRIx8
#define SCNXLEAST8      PRIX8

/** Format string macros for scanf() for int_least16_t/uint_least16_t. */
#define SCNdLEAST16     PRId16
#define SCNiLEAST16     PRIi16
#define SCNoLEAST16     PRIo16
#define SCNuLEAST16     PRIu16
#define SCNxLEAST16     PRIx16
#define SCNXLEAST16     PRIX16

/** Format string macros for scanf() for int_least32_t/uint_least32_t. */
#define SCNdLEAST32     PRId32
#define SCNiLEAST32     PRIi32
#define SCNoLEAST32     PRIo32
#define SCNuLEAST32     PRIu32
#define SCNxLEAST32     PRIx32
#define SCNXLEAST32     PRIX32

/** Format string macros for scanf() for int_least64_t/uint_least64_t. */
#define SCNdLEAST64     PRId64
#define SCNiLEAST64     PRIi64
#define SCNoLEAST64     PRIo64
#define SCNuLEAST64     PRIu64
#define SCNxLEAST64     PRIx64
#define SCNXLEAST64     PRIX64

/** Format string macros for scanf() for int_fast8_t/uint_fast8_t. */
#define SCNdFAST8       PRId8
#define SCNiFAST8       PRIi8
#define SCNoFAST8       PRIo8
#define SCNuFAST8       PRIu8
#define SCNxFAST8       PRIx8
#define SCNXFAST8       PRIX8

/** Format string macros for scanf() for int_fast16_t/uint_fast16_t. */
#define SCNdFAST16      PRId16
#define SCNiFAST16      PRIi16
#define SCNoFAST16      PRIo16
#define SCNuFAST16      PRIu16
#define SCNxFAST16      PRIx16
#define SCNXFAST16      PRIX16

/** Format string macros for scanf() for int_fast32_t/uint_fast32_t. */
#define SCNdFAST32      PRId32
#define SCNiFAST32      PRIi32
#define SCNoFAST32      PRIo32
#define SCNuFAST32      PRIu32
#define SCNxFAST32      PRIx32
#define SCNXFAST32      PRIX32

/** Format string macros for scanf() for int_fast64_t/uint_fast64_t. */
#define SCNdFAST64      PRId64
#define SCNiFAST64      PRIi64
#define SCNoFAST64      PRIo64
#define SCNuFAST64      PRIu64
#define SCNxFAST64      PRIx64
#define SCNXFAST64      PRIX64

/** Format string macros for scanf() for intptr_t/uintptr_t. */
#define SCNdPTR         "ld"
#define SCNiPTR         "li"
#define SCNoPTR         "lo"
#define SCNuPTR         "lu"
#define SCNxPTR         "lx"
#define SCNXPTR         "lX"

/** Format string macros for scanf() for intmax_t/uintmax_t. */
#define SCNdMAX         __PRI_MAX_MODIFIER "d"
#define SCNiMAX         __PRI_MAX_MODIFIER "i"
#define SCNoMAX         __PRI_MAX_MODIFIER "o"
#define SCNuMAX         __PRI_MAX_MODIFIER "u"
#define SCNxMAX         __PRI_MAX_MODIFIER "x"
#define SCNXMAX         __PRI_MAX_MODIFIER "X"

/* intmax_t imaxabs(intmax_t); */
/* imaxdiv_t imaxdiv(intmax_t, intmax_t); */
/* intmax_t strtoimax(const char *restrict, char **restrict, int); */
/* uintmax_t strtoumax(const char *restrict, char **restrict, int); */
/* intmax_t wcstoimax(const wchar_t *restrict, wchar_t **restrict, int); */
/* uintmax_t wcstoumax(const wchar_t *restrict, wchar_t **restrict, int); */

#ifdef __cplusplus
}
#endif

#endif /* __INTTYPES_H */
