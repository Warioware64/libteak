// SPDX-License-Identifier: CC0-1.0

#include "fixed.h"

int32_t q16_mul(int32_t a, int32_t b)
{
    // a = ah * 2^16 + al, with ah signed and al unsigned (same for b):
    //
    //   (a * b) >> 16 = ((ah * bh) << 16) + ah * bl + al * bh
    //                   + ((al * bl) >> 16)
    //
    // All terms fit in 32 bits, and the result is exact modulo 2^32.
    int32_t ah = a >> 16;
    int32_t bh = b >> 16;
    uint32_t al = (uint16_t)a;
    uint32_t bl = (uint16_t)b;

    uint32_t r = (uint32_t)(ah * bh) << 16;
    r += (uint32_t)(ah * (int32_t)bl);
    r += (uint32_t)((int32_t)al * bh);
    r += (al * bl) >> 16;

    return (int32_t)r;
}

uint32_t udiv32(uint32_t n, uint32_t d, uint32_t *rem)
{
    if (d == 0)
    {
        if (rem)
            *rem = n;
        return 0xFFFFFFFF;
    }

    uint32_t q = 0;
    uint32_t r = 0;

    // Long division, one bit per step, with constant shifts only
    for (int i = 0; i < 32; i++)
    {
        r = (r << 1) | (n >> 31);
        n <<= 1;
        q <<= 1;

        if (r >= d)
        {
            r -= d;
            q |= 1;
        }
    }

    if (rem)
        *rem = r;
    return q;
}

int32_t sdiv32(int32_t n, int32_t d)
{
    uint32_t un = (uint32_t)n;
    uint32_t ud = (uint32_t)d;
    int negative = 0;

    if (n < 0)
    {
        un = 0 - un;
        negative = 1;
    }
    if (d < 0)
    {
        ud = 0 - ud;
        negative = 1 - negative;
    }

    uint32_t q = udiv32(un, ud, 0);

    if (negative)
        q = 0 - q;

    return (int32_t)q;
}

uint32_t isqrt32(uint32_t x)
{
    uint32_t res = 0;
    uint32_t bit = (uint32_t)1 << 30;

    while (bit > x)
        bit >>= 2;

    while (bit != 0)
    {
        uint32_t t = res + bit;

        if (x >= t)
        {
            x -= t;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }

        bit >>= 2;
    }

    return res;
}
