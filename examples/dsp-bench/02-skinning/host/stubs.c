// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include "skinning.h"

void k_skin_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n)
{
    k_skin(v, b, out, n);
}

void k_skin2_asm(const kvert *v, const kbone *b, kvec3 *out, uint16_t n)
{
    k_skin(v, b, out, n);
}

void k_skin_q16_asm(const kvert_q16 *v, const kbone_q16 *b, kvec3_q16 *out,
                    uint16_t n)
{
    k_skin_q16(v, b, out, n);
}
