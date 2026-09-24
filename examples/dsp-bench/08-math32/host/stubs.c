// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include <stdint.h>

#include "math32.h"

void k_alu_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
{
    k_alu(a, b, out, n);
}

void k_mul_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
{
    k_mul(a, b, out, n);
}

void k_q16mul_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
{
    k_q16mul(a, b, out, n);
}

void k_div_asm(const int32_t *a, const int32_t *b, int32_t *out, uint16_t n)
{
    k_div(a, b, out, n);
}

void k_sqrt_asm(const int32_t *a, int32_t *out, uint16_t n)
{
    k_sqrt(a, out, n);
}
