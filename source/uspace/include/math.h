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
 * @brief		Math functions.
 */

#ifndef __MATH_H
#define __MATH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

#include <limits.h>

#if defined(__x86_64__) || defined(__i386__)

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
	u_int	ext_pad:16;
#if defined(__x86_64__)
	u_int	ext_padh;
#endif
};

#define	DBL_EXPBITS	11
#define	DBL_FRACHBITS	20
#define	DBL_FRACLBITS	32
#define	DBL_FRACBITS	52

#define	SNG_EXPBITS	8
#define	SNG_FRACBITS	23

#define	SNG_EXP_BIAS	127
#define	DBL_EXP_BIAS	1023
#define	EXT_EXP_BIAS	16383

#define	SNG_EXP_INFNAN	255
#define	DBL_EXP_INFNAN	2047
#define	EXT_EXP_INFNAN	32767

#else
# error "No math definitions for your arch"
#endif

/*
 * ANSI/POSIX
 */
extern char __infinity[];
#define HUGE_VAL        (*(double *)(void *)__infinity)

/*
 * C99
 */
typedef double        double_t;
typedef float        float_t;

#define HUGE_VALF        ((float)HUGE_VAL)
#define HUGE_VALL        ((long double)HUGE_VAL)
#define INFINITY        HUGE_VALF

extern char __nan[];
#define NAN                (*(float *)(void *)__nan)

#define FP_INFINITE        0x01
#define FP_NAN                0x02
#define FP_NORMAL        0x04
#define FP_SUBNORMAL        0x08
#define FP_ZERO                0x10

#define FP_ILOGB0       (-INT_MAX)
#define FP_ILOGBNAN     INT_MAX

#define fpclassify(x) \
        ((sizeof (x) == sizeof (float)) ? \
                __fpclassifyf(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __fpclassify(x) \
        :        __fpclassifyl(x))
#define isfinite(x) \
        ((sizeof (x) == sizeof (float)) ? \
                __isfinitef(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __isfinite(x) \
        :        __isfinitel(x))
#define isnormal(x) \
        ((sizeof (x) == sizeof (float)) ? \
                __isnormalf(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __isnormal(x) \
        :        __isnormall(x))
#define signbit(x) \
        ((sizeof (x) == sizeof (float)) ? \
                __signbitf(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __signbit(x) \
        :        __signbitl(x))

#define isgreater(x, y)                (!isunordered((x), (y)) && (x) > (y))
#define isgreaterequal(x, y)        (!isunordered((x), (y)) && (x) >= (y))
#define isless(x, y)                (!isunordered((x), (y)) && (x) < (y))
#define islessequal(x, y)        (!isunordered((x), (y)) && (x) <= (y))
#define islessgreater(x, y)        (!isunordered((x), (y)) && \
                                        ((x) > (y) || (y) > (x)))
#define isunordered(x, y)        (isnan(x) || isnan(y))

#define isinf(x) \
        ((sizeof (x) == sizeof (float)) ? \
                isinff(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __isinf(x) \
        :        __isinfl(x))
#define isnan(x) \
        ((sizeof (x) == sizeof (float)) ? \
                isnanf(x) \
        : (sizeof (x) == sizeof (double)) ? \
                __isnan(x) \
        :        __isnanl(x))

/*
 * XOPEN/SVID
 */
#define M_E                2.7182818284590452354        /* e */
#define M_LOG2E                1.4426950408889634074        /* log 2e */
#define M_LOG10E        0.43429448190325182765        /* log 10e */
#define M_LN2                0.69314718055994530942        /* log e2 */
#define M_LN10                2.30258509299404568402        /* log e10 */
#define M_PI                3.14159265358979323846        /* pi */
#define M_PI_2                1.57079632679489661923        /* pi/2 */
#define M_PI_4                0.78539816339744830962        /* pi/4 */
#define M_1_PI                0.31830988618379067154        /* 1/pi */
#define M_2_PI                0.63661977236758134308        /* 2/pi */
#define M_2_SQRTPI        1.12837916709551257390        /* 2/sqrt(pi) */
#define M_SQRT2                1.41421356237309504880        /* sqrt(2) */
#define M_SQRT1_2        0.70710678118654752440        /* 1/sqrt(2) */

#define MAXFLOAT        ((float)3.40282346638528860e+38)

extern int signgam;

#define HUGE                MAXFLOAT

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
#if 0
extern double scalbln(double, long int);
#endif

extern double cbrt(double);
extern double hypot(double, double);

extern double erf(double);
extern double erfc(double);
extern double lgamma(double);
extern double tgamma(double);

#if 0
extern double nearbyint(double);
#endif
extern double rint(double);
extern long int lrint(double);
extern long long int llrint(double);
extern double round(double);
//extern long int lround(double);
//extern long long int llround(double);
extern double trunc(double);

extern double remainder(double, double);
extern double remquo(double, double, int *);

extern double copysign(double, double);
extern double nan(const char *);
extern double nextafter(double, double);
#if 0
extern double nexttoward(double, long double);
#endif

extern double fdim(double, double);
extern double fmax(double, double);
extern double fmin(double, double);

#if 0
extern double fma(double, double, double);
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
#if 0
extern float scalblnf(float, long int);
#endif

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
#if 0
extern float nearbyintf(float);
#endif
extern float rintf(float);
extern long int lrintf(float);
extern long long int llrintf(float);
extern float roundf(float);
//extern long int lroundf(float);
//extern long long int llroundf(float);
extern float truncf(float);

extern float fmodf(float, float);
extern float remainderf(float, float);
extern float remquof(float, float, int *);

extern float copysignf(float, float);
extern float nanf(const char *);
extern float nextafterf(float, float);
#if 0
extern float nexttowardf(float, long double);
#endif

extern float fdimf(float, float);
extern float fmaxf(float, float);
extern float fminf(float, float);

#if 0
extern float fmaf(float, float, float);
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
extern int isinff(float);
extern int isnanf(float);

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
#if 0
extern long double acosl(long double);
extern long double asinl(long double);
extern long double atanl(long double);
extern long double atan2l(long double, long double);
extern long double cosl(long double);
extern long double sinl(long double);
extern long double tanl(long double);

extern long double acoshl(long double);
extern long double asinhl(long double);
extern long double atanhl(long double);
extern long double coshl(long double);
extern long double sinhl(long double);
extern long double tanhl(long double);

extern long double expl(long double);
extern long double exp2l(long double);
extern long double expm1l(long double);
extern long double frexpl(long double, int *);
extern int ilogbl(long double);
extern long double ldexpl(long double, int);
extern long double logl(long double);
extern long double log10l(long double);
extern long double log1pl(long double);
extern long double log2l(long double);
extern long double logbl(long double);
extern long double modfl(long double, long double *);
extern long double scalbnl(long double, int);
extern long double scalblnl(long double, long int);

extern long double cbrtl(long double);
extern long double fabsl(long double);
extern long double hypotl(long double, long double);
extern long double powl(long double, long double);
extern long double sqrtl(long double);

extern long double erfl(long double);
extern long double erfcl(long double);
extern long double lgammal(long double);
extern long double tgammal(long double);

extern long double ceill(long double);
extern long double floorl(long double);
extern long double nearbyintl(long double);
extern long double rintl(long double);
extern long int lrintl(long double);
extern long long int llrintl(long double);
extern long double roundl(long double);
extern long int lroundl(long double);
extern long long int llroundl(long double);
extern long double truncl(long double);

extern long double fmodl(long double, long double);
extern long double remainderl(long double, long double);
extern long double remquol(long double, long double, int *);

extern long double copysignl(long double, long double);
extern long double nanl(const char *);
extern long double nextafterl(long double, long double);
extern long double nexttowardl(long double, long double);

#endif
extern long double fdiml(long double, long double);
#if 0
extern long double fmaxl(long double, long double);
extern long double fminl(long double, long double);

extern long double fmal(long double, long double, long double);
#endif

/*
 * Library implementation
 */
extern int __fpclassify(double);
extern int __fpclassifyf(float);
extern int __fpclassifyl(long double);
extern int __isfinite(double);
extern int __isfinitef(float);
extern int __isfinitel(long double);
extern int __isinf(double);
extern int __isinfl(long double);
extern int __isnan(double);
extern int __isnanl(long double);
extern int __isnormal(double);
extern int __isnormalf(float);
extern int __isnormall(long double);
extern int __signbit(double);
extern int __signbitf(float);
extern int __signbitl(long double);

#ifdef __cplusplus
}
#endif

#endif /* __MATH_H */
