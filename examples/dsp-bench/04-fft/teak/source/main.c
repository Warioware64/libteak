// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the FFT example.

#include <stddef.h>

#include <teak/teak.h>

#include "fft.h"
#include "proto_teak.h"

void fft_bfly_asm(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups);

// Stages run by k_fft_asm() (PARAM_STAGES)
static uint16_t fft_stages = 8;

// Same as k_fft_stages(), with the assembly butterflies
static void k_fft_asm(const kcplx *in, kcplx *out)
{
    fft_bitrev_copy(in, out);

    uint16_t stages = fft_stages;
    uint16_t groups = FFT_N / 2;
    for (uint16_t half = 1; (half < FFT_N) && (stages > 0); half <<= 1)
    {
        for (uint16_t j = 0; j < half; j++)
            fft_bfly_asm(&out[j], &twiddle[2 * j * groups], half, groups);
        groups >>= 1;
        stages--;
    }
}

void app_init(void)
{
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    if ((table == TABLE_TWIDDLE) && (words <= TWIDDLE_WORDS))
        return (uint16_t *)twiddle;
    return NULL;
}

uint16_t app_job(uint16_t job)
{
    switch (job)
    {
        case JOB_SETUP:
            fft_setup();
            break;
        case JOB_FFT_C:
            k_fft(fft_in, fft_out);
            break;
        case JOB_FFT_ASM:
            k_fft_asm(fft_in, fft_out);
            break;
        case JOB_FFT2_ASM:
            k_fft2_asm(fft_in, fft_out);
            break;
        default:
            return PROTO_STATUS_BAD_ARG;
    }

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    return fft_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    (void)buffer;
    return proto_dma_out(fft_out, address, FFT_N * 2);
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    if ((id != PARAM_STAGES) || (value > 8))
        return PROTO_STATUS_BAD_ARG;

    fft_stages = (uint16_t)value;
    return PROTO_STATUS_OK;
}
