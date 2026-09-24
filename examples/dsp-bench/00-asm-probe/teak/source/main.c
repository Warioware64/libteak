// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the assembly probe: runs one snippet per job, and returns its
// result as the "checksum" of the job.

#include <stddef.h>

#include <teak/teak.h>

#include "probe.h"
#include "proto_teak.h"

int16_t pdata[32];

static const int16_t pdata_init[32] = {
    3, -5, 7, 11, -13, 17, 19, -23,
    29, 31, -37, 41, 43, -47, 53, 59,
    1000, -2000, 3000, -4000, 0x7FFF, -0x8000, 0x1234, -1,
    5, 6, 7, 8, 9, 10, 11, 12,
};

#define PROBE_DECLARE(name) uint32_t probe_##name(void);
PROBE_LIST(PROBE_DECLARE)

typedef struct {
    int16_t re, im;
} kcplx;

void fft_bfly_asm(kcplx *x, const int16_t *w, uint16_t half, uint16_t groups);

// FFT butterflies of the example 04 on known data: returns a checksum of the
// data after the butterflies
static kcplx fft_data[16];
static int16_t fft_w[2] = { 23170, 23170 }; // 45 degrees

static uint32_t fft_case(uint16_t half, uint16_t groups)
{
    for (int i = 0; i < 16; i++)
    {
        fft_data[i].re = (int16_t)(pdata[i] * 100);
        fft_data[i].im = (int16_t)(pdata[i + 16] >> 1);
    }

    fft_bfly_asm(fft_data, fft_w, half, groups);

    proto_checksum c;
    checksum_init(&c);
    for (int i = 0; i < 16; i++)
    {
        checksum_add16(&c, (uint16_t)fft_data[i].re);
        checksum_add16(&c, (uint16_t)fft_data[i].im);
    }
    return checksum_get(&c);
}

uint32_t probe_fft_h1_g1(void) { return fft_case(1, 1); }
uint32_t probe_fft_h1_g2(void) { return fft_case(1, 2); }
uint32_t probe_fft_h2_g1(void) { return fft_case(2, 1); }
uint32_t probe_fft_h1_g4(void) { return fft_case(1, 4); }
uint32_t probe_fft_h4_g2(void) { return fft_case(4, 2); }

static uint32_t result;

void app_init(void)
{
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    (void)table;
    (void)words;
    return NULL;
}

#define PROBE_CASE(name)                \
    case PROBE_##name:                  \
        result = probe_##name();        \
        break;

uint16_t app_job(uint16_t job)
{
    for (int i = 0; i < 32; i++)
        pdata[i] = pdata_init[i];

    switch (job)
    {
        PROBE_LIST(PROBE_CASE)
        default:
            return PROTO_STATUS_BAD_ARG;
    }

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    (void)job;
    return result;
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    (void)buffer;
    (void)address;
    return PROTO_STATUS_BAD_CMD;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
