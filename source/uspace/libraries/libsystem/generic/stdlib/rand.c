/*
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
 * @brief		Random number functions.
 */

/* Basically taken from newlib. Not sure what the license is on it, need
 * to find out. newlib/newlib/libc/stdlib/rand{,_r}.c */

#include <stdlib.h>

static unsigned long long __rand_next = 1;

/** Set random seed.
 *
 * Sets the seed for the rand() function to the given value.
 *
 * @param seed		New random seed.
 */
void srand(unsigned int seed) {
	__rand_next = seed;
}

/** Generate a random number.
 *
 * Generates a random number in the range [0,RAND_MAX) based on the current
 * random seed.
 *
 * @return		Generated random number.
 */
int rand(void) {
	/* This multiplier was obtained from Knuth, D.E., "The Art of
	   Computer Programming," Vol 2, Seminumerical Algorithms, Third
	   Edition, Addison-Wesley, 1998, p. 106 (line 26) & p. 108 */
	__rand_next = __rand_next * 6364136223846793005LL + 1;
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
