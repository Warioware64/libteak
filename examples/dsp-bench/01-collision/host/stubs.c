// SPDX-License-Identifier: CC0-1.0
//
// Host versions of the assembly functions, for tools/hostcheck/hostcheck.sh

#include "collision.h"

void k_dot3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n)
{
    k_dot3(a, b, out, n);
}

void k_cross3_asm(const kvec3 *a, const kvec3 *b, int32_t *out, uint16_t n)
{
    k_cross3(a, b, out, n);
}

void k_sphere_asm(const ksphere *a, const ksphere *b, uint16_t *mask,
                  uint16_t n)
{
    k_sphere(a, b, mask, n);
}

void k_aabb_asm(const ksphere *a, const ksphere *b, uint16_t *mask, uint16_t n)
{
    k_aabb(a, b, mask, n);
}

void k_dot3_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b, int32_t *out,
                    uint16_t n)
{
    k_dot3_q16(a, b, out, n);
}

void k_sphere_q16_asm(const ksphere_q16 *a, const ksphere_q16 *b,
                      uint16_t *mask, uint16_t n)
{
    k_sphere_q16(a, b, mask, n);
}
