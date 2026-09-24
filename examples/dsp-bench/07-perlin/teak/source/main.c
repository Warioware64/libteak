// SPDX-License-Identifier: CC0-1.0
//
// DSP side of the Perlin noise example.

#include <stddef.h>

#include <teak/teak.h>

#include "perlin.h"
#include "proto_teak.h"

void app_init(void)
{
}

uint16_t *app_upload_buffer(uint16_t table, uint16_t words)
{
    if ((table == TABLE_PERM) && (words <= PERM_ENTRIES))
        return perm;
    if ((table == TABLE_GRAD) && (words <= GRAD_ENTRIES * 2))
        return (uint16_t *)grad;
    return NULL;
}

uint16_t app_job(uint16_t job)
{
    switch (job)
    {
        case JOB_NOISE_C:
            k_noise(img_noise, 1);
            break;
        case JOB_FBM_C:
            k_noise(img_fbm, 4);
            break;
        case JOB_NOISE_Q16_C:
            k_noise_q16(img_noise_q16, 1);
            break;
        case JOB_FBM_Q16_C:
            k_noise_q16(img_fbm_q16, 4);
            break;
        case JOB_NOISE_ASM:
            k_noise_asm(img_noise);
            break;
        case JOB_FBM_ASM:
            k_fbm_asm(img_fbm);
            break;
        case JOB_NOISE_Q16_ASM:
            k_noise_q16_asm(img_noise_q16);
            break;
        case JOB_FBM_Q16_ASM:
            k_fbm_q16_asm(img_fbm_q16);
            break;
        case JOB_INPUTS:
            break;
        default:
            return PROTO_STATUS_BAD_ARG;
    }

    return PROTO_STATUS_OK;
}

uint32_t app_checksum(uint16_t job)
{
    return perlin_checksum(job);
}

uint16_t app_send(uint16_t buffer, uint32_t address)
{
    if (buffer == BUFFER_FBM)
        return proto_dma_out(img_fbm, address, IMG_SIZE);
    return PROTO_STATUS_BAD_ARG;
}

uint16_t app_param(uint16_t id, uint32_t value)
{
    (void)id;
    (void)value;
    return PROTO_STATUS_BAD_CMD;
}
