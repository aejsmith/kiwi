/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Random number functions.
 *
 * Basically taken from newlib. Not sure what the license is on it, need to
 * find out. newlib/newlib/libc/stdlib/rand{,_r}.c
 */

#include <stdlib.h>

static unsigned long long __rand_next = 1;

/** Set the seed for rand().
 * @param seed          New random seed. */
void srand(unsigned int seed) {
    __rand_next = seed;
}

/** Generate a random number in the range [0,RAND_MAX).
 * @return              Generated random number. */
int rand(void) {
    /* This multiplier was obtained from Knuth, D.E., "The Art of
       Computer Programming," Vol 2, Seminumerical Algorithms, Third
       Edition, Addison-Wesley, 1998, p. 106 (line 26) & p. 108 */
    __rand_next = __rand_next * 6364136223846793005ll + 1;
    return (int)((__rand_next >> 32) & RAND_MAX);
}

/* Pseudo-random generator based on Minimal Standard by
   Lewis, Goodman, and Miller in 1969.

   I[j+1] = a*I[j] (mod m)

   where a = 16807
         m = 2147483647

   Using Schrage's algorithm, a*I[j] (mod m) can be rewritten as:

     a*(I[j] mod q) - r*{I[j]/q}      if >= 0
     a*(I[j] mod q) - r*{I[j]/q} + m  otherwise

   where: {} denotes integer division
          q = {m/a} = 127773
          r = m (mod a) = 2836

   note that the seed value of 0 cannot be used in the calculation as
   it results in 0 itself
*/

int rand_r(unsigned int *seed) {
    long k;
    long s = (long)(*seed);
    if (s == 0)
        s = 0x12345987;
    k = s / 127773;
    s = 16807 * (s - k * 127773) - 2836 * k;
    if (s < 0)
        s += 2147483647;
    (*seed) = (unsigned int)s;
    return (int)(s & RAND_MAX);
}
