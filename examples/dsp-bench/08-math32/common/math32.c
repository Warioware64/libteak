// SPDX-License-Identifier: CC0-1.0

#include "fixed.h"
#include "math32.h"

int32_t val_a[N_VALUES];
int32_t val_b[N_VALUES];
int32_t val_out[N_VALUES];

static uint16_t rng_state;

static uint16_t rng(void)
{
    uint16_t s = rng_state;
    s ^= (uint16_t)(s << 7);
    s ^= s >> 9;
    s ^= (uint16_t)(s << 8);
    rng_state = s;
    return s;
}

static uint32_t rng32(void)
{
    uint32_t hi = rng();
    return (hi << 16) | rng();
}

void math32_generate(void)
{
    rng_state = 0x3232;

    for (int i = 0; i < N_VALUES; i++)
    {
        val_a[i] = (int32_t)rng32();

        // Values of different magnitudes for the divisions
        int32_t b = (int32_t)rng32();
        uint16_t shift = rng() & 0x1F;
        for (uint16_t s = 0; s < shift; s++)
            b >>= 1;
        if (b == 0)
            b = 7;
        val_b[i] = b;
    }
}

void k_alu(const int32_t *a, const int32_t *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = ((a[i] + b[i]) >> 3) ^ (a[i] - (int32_t)((uint32_t)b[i] << 2));
}

void k_mul(const int32_t *a, const int32_t *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)((uint32_t)a[i] * (uint32_t)b[i]);
}

void k_q16mul(const int32_t *a, const int32_t *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = q16_mul(a[i], b[i]);
}

void k_div(const int32_t *a, const int32_t *b, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = sdiv32(a[i], b[i]);
}

void k_sqrt(const int32_t *a, int32_t *out, int n)
{
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)isqrt32((uint32_t)a[i]);
}

uint32_t math32_checksum(math32_job job)
{
    proto_checksum c;
    checksum_init(&c);

    if (job == JOB_INPUTS)
    {
        for (int i = 0; i < N_VALUES; i++)
        {
            checksum_add32(&c, (uint32_t)val_a[i]);
            checksum_add32(&c, (uint32_t)val_b[i]);
        }
    }
    else
    {
        for (int i = 0; i < N_VALUES; i++)
            checksum_add32(&c, (uint32_t)val_out[i]);
    }

    return checksum_get(&c);
}
