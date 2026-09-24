// SPDX-License-Identifier: CC0-1.0
//
// Input generator and reference kernels. The same source is built for the ARM9
// and for the DSP.
// They are in their own file so that the compiler can't optimize the calls
// away when they are repeated.

#include "bench.h"

void bench_fill(int16_t *buffer, uint16_t n, uint16_t seed)
{
    uint16_t s = seed;

    for (uint16_t i = 0; i < n; i++)
    {
        s ^= (uint16_t)(s << 7);
        s ^= s >> 9;
        s ^= (uint16_t)(s << 8);
        buffer[i] = (int16_t)(s & 0x7FF) - 1024;
    }
}

uint32_t bench_dot16_c(const int16_t *a, const int16_t *b, uint16_t n)
{
    // Unsigned accumulator: wrapping is well defined on both CPUs
    uint32_t sum = 0;

    for (uint16_t i = 0; i < n; i++)
        sum += (uint32_t)((int32_t)a[i] * b[i]);

    return sum;
}

uint32_t bench_sum16_c(const uint16_t *a, uint16_t n)
{
    uint32_t sum = 0;

    for (uint16_t i = 0; i < n; i++)
        sum += a[i];

    return sum;
}

uint32_t bench_xorshift16_c(uint16_t seed, uint16_t steps)
{
    uint16_t s = seed;
    uint16_t acc = 0;

    for (uint16_t i = 0; i < steps; i++)
    {
        s ^= (uint16_t)(s << 7);
        s ^= s >> 9;
        s ^= (uint16_t)(s << 8);
        acc ^= s;
    }

    return ((uint32_t)acc << 16) | s;
}
