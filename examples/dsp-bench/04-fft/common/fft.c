// SPDX-License-Identifier: CC0-1.0

#include "fft.h"

int16_t twiddle[TWIDDLE_WORDS];

kcplx fft_in[FFT_N];
kcplx fft_out[FFT_N];

uint16_t fft_bitrev[FFT_N];

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

// cos and sin of 2 pi m / N for any m, from the table of the first half
static int16_t tw_cos(uint16_t m)
{
    m &= FFT_N - 1;
    if (m < FFT_N / 2)
        return twiddle[2 * m];
    return (int16_t)-twiddle[2 * (m - FFT_N / 2)];
}

static int16_t tw_sin(uint16_t m)
{
    m &= FFT_N - 1;
    if (m < FFT_N / 2)
        return twiddle[2 * m + 1];
    return (int16_t)-twiddle[2 * (m - FFT_N / 2) + 1];
}

void fft_setup(void)
{
    for (uint16_t i = 0; i < FFT_N; i++)
    {
        uint16_t r = 0;
        uint16_t v = i;
        for (int b = 0; b < 8; b++)
        {
            r = (uint16_t)((r << 1) | (v & 1));
            v >>= 1;
        }
        fft_bitrev[i] = r;
    }

    // Two tones plus some noise
    rng_state = 0xF0F0;
    for (uint16_t n = 0; n < FFT_N; n++)
    {
        fft_in[n].re = (int16_t)((tw_cos((uint16_t)(5 * n)) >> 2)
                     + (tw_sin((uint16_t)(37 * n)) >> 3)
                     + (int16_t)(rng() & 0x3FF) - 0x200);
        fft_in[n].im = (int16_t)(tw_sin((uint16_t)(9 * n)) >> 2);
    }
}

void fft_bitrev_copy(const kcplx *in, kcplx *out)
{
    for (int i = 0; i < FFT_N; i++)
        out[fft_bitrev[i]] = in[i];
}

void fft_bfly(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups)
{
    int16_t c = w[0];
    int16_t s = w[1];

    for (uint16_t g = 0; g < groups; g++)
    {
        kcplx *a = x;
        kcplx *b = x + half;

        // b * (c - i s)
        int32_t tr = ((int32_t)b->re * c + (int32_t)b->im * s) >> 15;
        int32_t ti = ((int32_t)b->im * c - (int32_t)b->re * s) >> 15;
        int32_t ur = a->re;
        int32_t ui = a->im;

        a->re = (int16_t)((ur + tr) >> 1);
        a->im = (int16_t)((ui + ti) >> 1);
        b->re = (int16_t)((ur - tr) >> 1);
        b->im = (int16_t)((ui - ti) >> 1);

        x += 2 * half;
    }
}

void k_fft_stages(const kcplx *in, kcplx *out, uint16_t stages)
{
    fft_bitrev_copy(in, out);

    uint16_t groups = FFT_N / 2;
    for (uint16_t half = 1; (half < FFT_N) && (stages > 0); half <<= 1)
    {
        // Twiddle index of butterfly j is j * groups
        for (uint16_t j = 0; j < half; j++)
            fft_bfly(&out[j], &twiddle[2 * j * groups], half, groups);
        groups >>= 1;
        stages--;
    }
}

void k_fft(const kcplx *in, kcplx *out)
{
    k_fft_stages(in, out, 8);
}

uint32_t fft_checksum(fft_job job)
{
    proto_checksum c;
    checksum_init(&c);

    const kcplx *data = (job == JOB_SETUP) ? fft_in : fft_out;
    for (int i = 0; i < FFT_N; i++)
    {
        checksum_add16(&c, (uint16_t)data[i].re);
        checksum_add16(&c, (uint16_t)data[i].im);
    }

    return checksum_get(&c);
}
