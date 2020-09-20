/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * @file
 * @brief		Math functions.
 */

#pragma once

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__) || defined(__i386__)

#define	SNG_EXPBITS	8
#define	SNG_FRACBITS	23

#define	DBL_EXPBITS	11
#define	DBL_FRACHBITS	20
#define	DBL_FRACLBITS	32
#define	DBL_FRACBITS	52

#define	EXT_EXPBITS	15
#define	EXT_FRACHBITS	32
#define	EXT_FRACLBITS	32
#define	EXT_FRACBITS	64

#define	EXT_TO_ARRAY32(p, a) do {		\
	(a)[0] = (uint32_t)(p)->ext_fracl;	\
	(a)[1] = (uint32_t)(p)->ext_frach;	\
} while(0)

struct ieee_single {
	u_int	sng_frac:23;
	u_int	sng_exp:8;
	u_int	sng_sign:1;
};

struct ieee_double {
	u_int	dbl_fracl;
	u_int	dbl_frach:20;
	u_int	dbl_exp:11;
	u_int	dbl_sign:1;
};

struct ieee_ext {
	u_int	ext_fracl;
	u_int	ext_frach;
	u_int	ext_exp:15;
	u_int	ext_sign:1;
#ifdef __x86_64__
	u_int	ext_padl:16;
	u_int	ext_padh;
#else
	u_int	ext_pad:16;
#endif
};

#define	SNG_EXP_INFNAN	255
#define	DBL_EXP_INFNAN	2047
#define	EXT_EXP_INFNAN	32767

#define	SNG_EXP_BIAS	127
#define	DBL_EXP_BIAS	1023
#define	EXT_EXP_BIAS	16383

#else
# error "No math definitions for your arch"
#endif

/*
 * ANSI/POSIX
 */
extern char __infinity[];
#define HUGE_VAL	(*(double *)(void *)__infinity)

/*
 * C99
 */
typedef double	double_t;
typedef	float	float_t;

#ifdef __vax__
extern char __infinityf[];
#define	HUGE_VALF	(*(float *)(void *)__infinityf)
#else /* __vax__ */
#define	HUGE_VALF	((float)HUGE_VAL)
#endif /* __vax__ */
#define	HUGE_VALL	((long double)HUGE_VAL)
#define	INFINITY	HUGE_VALF
#ifndef __vax__
extern char __nan[];
#define	NAN		(*(float *)(void *)__nan)
#endif /* !__vax__ */

#define	FP_INFINITE	0x01
#define	FP_NAN		0x02
#define	FP_NORMAL	0x04
#define	FP_SUBNORMAL	0x08
#define	FP_ZERO		0x10

#define FP_ILOGB0	(-INT_MAX)
#define FP_ILOGBNAN	INT_MAX

#define fpclassify(x)           __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x)             __builtin_isfinite(x)
#define isnormal(x)             __builtin_isnormal(x)
#define signbit(x)              __builtin_signbit(x)
#define	isgreater(x, y)		    __builtin_isgreater(x, y)
#define	isgreaterequal(x, y)	__builtin_isgreaterequal(x, y)
#define	isless(x, y)		    __builtin_isless(x, y)
#define	islessequal(x, y)	    __builtin_islessequal(x, y)
#define	islessgreater(x, y)     __builtin_islessgreater(x, y)
#define	isunordered(x, y)	    __builtin_isunordered(x, y)
#define isinf(x)                __builtin_isinf(x)
#define isnan(x)                __builtin_isnan(x)

/*
 * XOPEN/SVID
 */
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#ifdef __vax__
#define	MAXFLOAT	((float)1.70141173319264430e+38)
#else
#define	MAXFLOAT	((float)3.40282346638528860e+38)
#endif /* __vax__ */

extern int signgam;

#define	HUGE		MAXFLOAT

/*
 * ANSI/POSIX
 */
extern double acos(double);
extern double asin(double);
extern double atan(double);
extern double atan2(double, double);
extern double cos(double);
extern double sin(double);
extern double tan(double);

extern double cosh(double);
extern double sinh(double);
extern double tanh(double);

extern double exp(double);
extern double frexp(double, int *);
extern double ldexp(double, int);
extern double log(double);
extern double log10(double);
extern double modf(double, double *);

extern double pow(double, double);
extern double sqrt(double);

extern double ceil(double);
extern double fabs(double);
extern double floor(double);
extern double fmod(double, double);

/*
 * C99
 */
extern double acosh(double);
extern double asinh(double);
extern double atanh(double);

extern double exp2(double);
extern double expm1(double);
extern int ilogb(double);
extern double log1p(double);
extern double log2(double);
extern double logb(double);
extern double scalbn(double, int);
extern double scalbln(double, long int);

extern double cbrt(double);
extern double hypot(double, double);

extern double erf(double);
extern double erfc(double);
extern double lgamma(double);
extern double tgamma(double);

#ifdef __cplusplus
double nearbyint(double);
#endif
extern double rint(double);
extern long int lrint(double);
extern long long int llrint(double);
extern double round(double);
extern long int lround(double);
extern long long int llround(double);
extern double trunc(double);

extern double remainder(double, double);
extern double remquo(double, double, int *);

extern double copysign(double, double);
extern double nan(const char *);
extern double nextafter(double, double);
#ifdef __cplusplus
double nexttoward(double, long double);
#endif

extern double fdim(double, double);
extern double fmax(double, double);
extern double fmin(double, double);

#ifdef __cplusplus
double fma(double, double, double);
#endif

extern double j0(double);
extern double j1(double);
extern double jn(int, double);
extern double scalb(double, double);
extern double y0(double);
extern double y1(double);
extern double yn(int, double);

extern double gamma(double);

/*
 * BSD math library entry points
 */
extern double drem(double, double);
extern int finite(double);

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
extern double gamma_r(double, int *);
extern double lgamma_r(double, int *);

/*
 * IEEE Test Vector
 */
extern double significand(double);

/*
 * Float versions of C99 functions
 */
extern float acosf(float);
extern float asinf(float);
extern float atanf(float);
extern float atan2f(float, float);
extern float cosf(float);
extern float sinf(float);
extern float tanf(float);

extern float acoshf(float);
extern float asinhf(float);
extern float atanhf(float);
extern float coshf(float);
extern float sinhf(float);
extern float tanhf(float);

extern float expf(float);
extern float exp2f(float);
extern float expm1f(float);
extern float frexpf(float, int *);
extern int ilogbf(float);
extern float ldexpf(float, int);
extern float logf(float);
extern float log10f(float);
extern float log1pf(float);
extern float log2f(float);
extern float logbf(float);
extern float modff(float, float *);
extern float scalbnf(float, int);
extern float scalblnf(float, long int);

extern float cbrtf(float);
extern float fabsf(float);
extern float hypotf(float, float);
extern float powf(float, float);
extern float sqrtf(float);

extern float erff(float);
extern float erfcf(float);
extern float lgammaf(float);
extern float tgammaf(float);

extern float ceilf(float);
extern float floorf(float);
#ifdef __cplusplus
float nearbyintf(float);
#endif
extern float rintf(float);
extern long int lrintf(float);
extern long long int llrintf(float);
extern float roundf(float);
extern long int lroundf(float);
extern long long int llroundf(float);
extern float truncf(float);

extern float fmodf(float, float);
extern float remainderf(float, float);
extern float remquof(float, float, int *);

extern float copysignf(float, float);
extern float nanf(const char *);
extern float nextafterf(float, float);
#ifdef __cplusplus
float nexttowardf(float, long double);
#endif

extern float fdimf(float, float);
extern float fmaxf(float, float);
extern float fminf(float, float);

#ifdef __cplusplus
float fmaf(float, float, float);
#endif

extern float j0f(float);
extern float j1f(float);
extern float jnf(int, float);
extern float scalbf(float, float);
extern float y0f(float);
extern float y1f(float);
extern float ynf(int, float);

extern float gammaf(float);

/*
 * Float versions of BSD math library entry points
 */
extern float dremf(float, float);
extern int finitef(float);

/*
 * Float versions of reentrant version of gamma & lgamma; passes
 * signgam back by reference as the second argument; user must
 * allocate space for signgam.
 */
extern float gammaf_r(float, int *);
extern float lgammaf_r(float, int *);

/*
 * Float version of IEEE Test Vector
 */
extern float significandf(float);

/*
 * Long double versions of C99 functions
 */
extern long double acosl(long double);
extern long double asinl(long double);
extern long double atanl(long double);
extern long double atan2l(long double, long double);
extern long double cosl(long double);
extern long double sinl(long double);
extern long double tanl(long double);

#ifdef __cplusplus
long double acoshl(long double);
long double asinhl(long double);
long double atanhl(long double);
long double coshl(long double);
long double sinhl(long double);
long double tanhl(long double);
#endif

#ifdef __cplusplus
long double expl(long double);
#endif
extern long double exp2l(long double);
#ifdef __cplusplus
long double expm1l(long double);
#endif
extern long double frexpl(long double, int *);
extern int ilogbl(long double);
extern long double ldexpl(long double, int);
#ifdef __cplusplus
long double logl(long double);
long double log10l(long double);
long double log1pl(long double);
long double log2l(long double);
#endif
extern long double logbl(long double);
#ifdef __cplusplus
long double modfl(long double, long double *);
#endif
extern long double scalbnl(long double, int);
extern long double scalblnl(long double, long int);

#ifdef __cplusplus
long double cbrtl(long double);
#endif
extern long double fabsl(long double);
#ifdef __cplusplus
long double hypotl(long double, long double);
long double powl(long double, long double);
#endif
extern long double sqrtl(long double);

#ifdef __cplusplus
long double erfl(long double);
long double erfcl(long double);
long double lgammal(long double);
long double tgammal(long double);
#endif

#ifdef __cplusplus
long double ceill(long double);
long double floorl(long double);
long double nearbyintl(long double);
#endif
extern long double rintl(long double);
#ifdef __cplusplus
long int lrintl(long double);
long long int llrintl(long double);
long double roundl(long double);
long int lroundl(long double);
long long int llroundl(long double);
long double truncl(long double);
#endif

#ifdef __cplusplus
long double fmodl(long double, long double);
long double remainderl(long double, long double);
long double remquol(long double, long double, int *);
#endif

extern long double copysignl(long double, long double);
extern long double nanl(const char *);
#ifdef __cplusplus
long double nextafterl(long double, long double);
long double nexttowardl(long double, long double);
#endif

extern long double fdiml(long double, long double);
extern long double fmaxl(long double, long double);
extern long double fminl(long double, long double);

#ifdef __cplusplus
long double fmal(long double, long double, long double);
#endif

/*
 * Library implementation
 */
extern int __fpclassify(double);
extern int __fpclassifyf(float);
extern int __fpclassifyl(long double);

#if defined(__vax__)
extern double infnan(int);
#endif

#ifdef __cplusplus
}
#endif
