/* Kiwi x86 optimized string functions
 * Copyright (C) 2008 Alex Smith
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
 * @brief		x86 optimized string functions.
 */

#include <arch/barrier.h>
#include <arch/x86/features.h>

#include <lib/string.h>

#include <assert.h>

/** Save the MMX registers we're going to change. */
#define MMX_SAVE_1() \
	uint8_t __mmx_save[8]; \
	__asm__ __volatile__( \
		"movq %%mm0, (%0)\n\t" \
		: \
		: "r"(__mmx_save) \
		: "memory" \
	)

/** Restore the original contents of MMX registers we've changed. */
#define MMX_RESTORE_1() \
	__asm__ volatile( \
		"movq (%0), %%mm0\n\t" \
		: \
		: "r"(__mmx_save) \
		: "memory" \
	)

/** Save the MMX registers we're going to change. */
#define MMX_SAVE_4() \
	uint8_t __mmx_save[32]; \
	__asm__ __volatile__( \
		"movq %%mm0,  0(%0)\n\t" \
		"movq %%mm1,  8(%0)\n\t" \
		"movq %%mm2, 16(%0)\n\t" \
		"movq %%mm3, 24(%0)\n\t" \
		: \
		: "r"(__mmx_save) \
		: "memory" \
	)

/** Restore the original contents of MMX registers we've changed. */
#define MMX_RESTORE_4() \
	__asm__ volatile( \
		"movq  0(%0), %%mm0\n\t" \
		"movq  8(%0), %%mm1\n\t" \
		"movq 16(%0), %%mm2\n\t" \
		"movq 24(%0), %%mm3\n\t" \
		: \
		: "r"(__mmx_save) \
		: "memory" \
	)

/** Small memcpy() using rep movsb. */
#define __small_memcpy(dest, src, count) \
	{ \
		register size_t out; \
		__asm__ volatile( \
			"rep movsb" \
			: "=&D"(dest), "=&S"(src), "=&c"(out) \
			: "0"(dest), "1"(src), "2"(count) \
			: "memory" \
		); \
	}

/** Small memset() using rep stosb. */
#define __small_memset(dest, val, count) \
	{ \
		register size_t out; \
		__asm__ volatile( \
			"rep stosb" \
			: "=&D"(dest), "=&c"(out) \
			: "a"(val), "0"(dest), "1"(count) \
			: "memory" \
		); \
	}

/** Helper for memcpy() to copy the final part of a block or small blocks.
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy. */
static inline void __memcpy_helper(void *dest, const void *src, size_t count) {
	int d0, d1, d2;

	if(count < 4) {
		__small_memcpy(dest, src, count);
		return;
	}

	__asm__ volatile(
		"rep movsl\n\t"
		"testb $2, %b4\n\t"
		"je 1f\n\t"
		"movsw\n"
		"1: testb $1, %b4\n\t"
		"je 2f\n\t"
		"movsb\n"
		"2:"
		: "=&c"(d0), "=&D"(d1), "=&S"(d2)
		: "0"(count / 4), "q"(count), "1"(dest), "2"(src)
		: "memory"
	);
}

/** Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 */
void *memcpy(void *dest, const void *src, size_t count) {
	char *ret = dest;

	if(CPU_HAS_MMX(curr_cpu) && count >= 64) {
		/* Align destination to 16-byte boundary if 512 bytes or
		 * larger. During testing of this function I found that doing
		 * this on smaller chunks had a detrimental effect on
		 * performance. However, doing this for larger chunks helps. */
		if(count >= 512) {
			size_t delta;

			delta = (ptr_t)dest & 15;
			if(delta) {
				delta = 16 - delta;
				count -= delta;

				__memcpy_helper(dest, src, delta);

				dest += delta;
				src += delta;
			}
		}

		/* Copy the data 64 bytes at a time. */
		if(count >= 64) {
			size_t i;

			i = count >> 6;
			count &= 63;

			MMX_SAVE_4();

			for(; i > 0; i--, src += 64, dest += 64) {
		      		__asm__ __volatile__(
					"movq  0(%0), %%mm0\n\t"
					"movq  8(%0), %%mm1\n\t"
					"movq 16(%0), %%mm2\n\t"
					"movq 24(%0), %%mm3\n\t"

					"movq %%mm0,  0(%1)\n\t"
					"movq %%mm1,  8(%1)\n\t"
					"movq %%mm2, 16(%1)\n\t"
					"movq %%mm3, 24(%1)\n\t"

					"movq 32(%0), %%mm0\n\t"
					"movq 40(%0), %%mm1\n\t"
					"movq 48(%0), %%mm2\n\t"
					"movq 56(%0), %%mm3\n\t"

					"movq %%mm0, 32(%1)\n\t"
					"movq %%mm1, 40(%1)\n\t"
					"movq %%mm2, 48(%1)\n\t"
					"movq %%mm3, 56(%1)\n\t"
					:
					: "r"(src), "r"(dest)
					: "memory"
				);
			}

			write_barrier();
			MMX_RESTORE_4();
		}
	}

	/* Copy the remainder. */
	__memcpy_helper(dest, src, count);
	return ret;
}

/** Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 */
void *memmove(void *dest, const void *src, size_t count) {
	int d0, d1, d2;

	if(src == dest) {
		return dest;
	} else if(src > dest) {
		return memcpy(dest, src, count);
	} else {
		__asm__ volatile(
			"std\n\t"
			"rep movsb\n\t"
			"cld\n\t"
			: "=&c"(d0), "=&S"(d1), "=&D"(d2)
			: "0"(count), "1"(src + (count - 1)), "2"(dest + (count - 1))
			: "memory"
		);
	}

	return dest;
}

/** Fill a memory area.
 *
 * Fills a memory area with the value specified.
 *
 * @param dest		The memory area to fill.
 * @param val		The value to fill with.
 * @param count		The number of bytes to fill.
 */
void *memset(void *dest, int val, size_t count) {
	void *ret = dest;

	if(count == 0) {
		return ret;
	} else if(count == 1) {
		*(char *)dest = val;
		return ret;
	}

	if(CPU_HAS_MMX(curr_cpu) && count >= 64) {
		/* Align destination to 16-byte boundary if 512 bytes or
		 * larger (see comment in memcpy()). */
		if(count >= 512) {
			size_t delta;

			delta = (ptr_t)dest & 15;
			if(delta) {
				delta = 16 - delta;
				count -= delta;

				__small_memset(dest, val, delta);
			}
		}

		/* Copy the data 64 bytes at a time. */
		if(count >= 64) {
			uint32_t _val = 0;
			size_t i;

			i = count >> 6;
			count &= 63;

			MMX_SAVE_1();

			/* Load MM0 with the correct value. */
			if(val != 0) {
				_val = (uint32_t)val * 0x01010101;
				__asm__ __volatile__(
					"movd (%0), %%mm0\n\t"
					"punpcklbw %%mm0, %%mm0\n\t"
					:
					: "r"((ptr_t)&_val)
				);
			} else {
				__asm__ __volatile__("movd (%0), %%mm0\n\t" :: "r"((ptr_t)&_val));
			}

			for(; i > 0; i--, dest += 64) {
		      		__asm__ __volatile__(
					"movq %%mm0,  0(%0)\n\t"
					"movq %%mm0,  8(%0)\n\t"
					"movq %%mm0, 16(%0)\n\t"
					"movq %%mm0, 24(%0)\n\t"
					"movq %%mm0, 32(%0)\n\t"
					"movq %%mm0, 40(%0)\n\t"
					"movq %%mm0, 48(%0)\n\t"
					"movq %%mm0, 56(%0)\n\t"
					:
					: "r"(dest)
					: "memory"
				);
			}

			write_barrier();
			MMX_RESTORE_1();
		}
	}

	__small_memset(dest, val, count);
	return ret;
}
