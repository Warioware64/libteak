// SPDX-License-Identifier: CC0-1.0
//
// 32-bit helpers built for both CPUs. The Teak compiler can't compute the high
// half of a 32x32 product (needed by Q16.16 multiplication) and has no
// division, so they are written with 16-bit pieces, constant shifts and
// comparisons only. The results are the same on both CPUs.

#ifndef FIXED_H__
#define FIXED_H__

#include <stdint.h>

// Q16.16 multiplication: ((int64_t)a * b) >> 16, truncated to 32 bits
int32_t q16_mul(int32_t a, int32_t b);

// Unsigned division. Returns 0xFFFFFFFF (and remainder n) if d is 0.
uint32_t udiv32(uint32_t n, uint32_t d, uint32_t *rem);

// Signed division rounded towards zero, like the C operator
int32_t sdiv32(int32_t n, int32_t d);

// Integer square root: floor(sqrt(x))
uint32_t isqrt32(uint32_t x);

#endif // FIXED_H__
